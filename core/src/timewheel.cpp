/*
 * =====================================================================================
 *
 *       Filename:  timewheel.cpp
 *
 *    Description:  时间轮, 实现定时和超时
 *
 *        Version:  1.0
 *        Created:  2014年05月06日 06时26分36秒
 *       Revision:  none
 *       Compiler:  gcc
 *
 *         Author:  piboye
 *   Organization:
 *
 * =====================================================================================
 */
#include <stdlib.h>
#include <sys/timerfd.h>  
#include <sys/time.h>
#include <sys/epoll.h>
#include <sys/syscall.h>
#include <assert.h>
#include <stdio.h>
#include <fcntl.h>
#include <sys/eventfd.h>

#include "gflags/gflags.h"
#include "hook_helper.h"
#include "timewheel.h"
#include "log.h"
#include "dispatch.h"
#include "conet_all.h"
#include "fd_ctx.h"
#include "coroutine.h"
#include "coroutine_env.h"

#include "base/tls.h"
#include "base/time_helper.h"

namespace conet
{

static inline uint64_t get_cur_ms()
{
    return get_sys_ms();
}

using namespace conet;

bool set_timeout_impl(timewheel_t *tw, timeout_handle_t * obj, int timeout, int interval);


void init_timewheel(timewheel_t *self, int solt_num = 60*1000);
void fini_timewheel(timewheel_t *self);


void init_timeout_handle(timeout_handle_t * self, void (*fn)(void *), void *arg)
{
    INIT_LIST_HEAD(&self->link_to);
    self->timeout = 0;
    self->fn = fn;
    self->arg = arg;
    self->tw = NULL;
    self->interval = 0;
}

static
int do_now_task(void *arg)
{
    timewheel_t *tw = (timewheel_t *)(arg);
    timeout_handle_t *it=NULL, *next=NULL;

    LIST_HEAD(queue);

    list_swap(&queue, &tw->now_list);

    list_for_each_entry_safe(it, next, &queue, link_to)
    {
      list_del_init(&it->link_to);
      it->fn(it->arg);
      if ( it->interval)
      {
          set_timeout_impl(tw, it, it->interval, it->interval);
      }
    }
    return 0;
}

timewheel_t::timewheel_t(coroutine_env_t *env, int slot_num)
{
    assert(slot_num > 0);
    list_head *slots = new list_head[slot_num];
    for(int i=0; i<slot_num; ++i) {
        INIT_LIST_HEAD(&slots[i]);
    }
    this->slots = slots;
    this->slot_num = slot_num;
    this->task_num = 0;

    uint64_t cur_ms = get_cur_ms();
    this->now_ms = cur_ms;
    this->pos = cur_ms % slot_num;
    this->prev_ms = cur_ms;
    this->stop_flag = 0;
    this->co = NULL;
    //self->notify = time_mgr_t::instance().alloc_timeout_notify();
    //self->enable_notify = 1;

    this->co_env = env;

    INIT_LIST_HEAD(&this->now_list);
    init_task(&this->delay_task, &do_now_task, this);

    // now 任务, 都放到 dispatch 的 delay 队列上
    co_env->dispatch->delay(&this->delay_task);
}


timewheel_t::~timewheel_t()
{
    co_env->dispatch->unregistry_delay(&this->delay_task);
    delete [] this->slots;
}



uint64_t get_latest_ms(timewheel_t *tw);

static 
int create_timer_fd()
{
    int timerfd = -1;
    timerfd = timerfd_create(CLOCK_MONOTONIC,  TFD_NONBLOCK | TFD_CLOEXEC);
    if (timerfd < 0) {
        LOG(ERROR)<<"timerfd_create failed, "
            "[ret:"<<timerfd<<"]"
            "[errno:"<<errno<<"]"
            "[errmsg:"<<strerror(errno)<<"]"
            ;
        return -1;
    }
    int ret = 0; 

    struct itimerspec ts;
    ts.it_value.tv_sec = 0;       //
    ts.it_value.tv_nsec = 1000000; // 1ms
    ts.it_interval.tv_sec = 0;
    ts.it_interval.tv_nsec = 1000000; //1ms

    ret = timerfd_settime(timerfd, 0, &ts, NULL);
    if (ret < 0) {
        LOG(ERROR)<<"timerfd_settime failed, "
            "[ret:"<<ret<<"]"
            "[errno:"<<errno<<"]"
            "[errmsg:"<<strerror(errno)<<"]"
            ;
        return -3;
    }

    return timerfd;
}

static
int timewheel_task(void *arg)
{
    conet::enable_sys_hook(); 
    timewheel_t *tw = (timewheel_t *)arg;
    int timerfd = -1;
    timerfd = create_timer_fd();
    tw->timerfd = timerfd;

    uint64_t cnt = 0;
    int ret = 0;

    tw->now_ms = get_cur_ms();

    while (!tw->stop_flag) {
       struct pollfd pf = { fd: timerfd, events: POLLIN | POLLERR | POLLHUP };
       ret = co_poll(&pf, 1, -1);

       cnt = 0;
       ret = syscall(SYS_read, timerfd, &cnt, sizeof(cnt)); 
       if (tw->stop_flag) {
        // 时间轮退出
            break;
       }
       if (ret != sizeof(cnt)) {
          LOG(ERROR)<<" timewheel timerfd read failed, "
               "[ret:"<<ret<<"]"
               "[timerfd:"<<timerfd<<"]"
               "[poll event:"<<pf.revents<<"]"
               "[errno:"<<errno<<"]"
               "[errmsg:"<<strerror(errno)<<"]"
               ;
           continue;
       }

       tw->now_ms = get_cur_ms(); 

       tw->check(tw->now_ms);
    }
    close(timerfd);
    tw->timerfd = -1;
    return 0;
}

static int timewheel_task2(void *arg)
{
    conet::enable_sys_hook(); 
    timewheel_t *tw = (timewheel_t *)arg;
    int evfd = eventfd(0, EFD_NONBLOCK | EFD_CLOEXEC);
    if (evfd < 0) {
        LOG(FATAL)<<"create eventfd failed! [ret:"<<evfd<<"]"
            ;
        return -1;
    }

    tw->event_fd = evfd;
    tw->notify->event_fd = evfd;

    time_mgr_t::instance().add_to_queue(tw->notify);

    tw->now_ms = get_cur_ms(); 
    int ret = 0;
    uint64_t cnt = 0;
    while (!tw->stop_flag)
    {
       struct pollfd pf = { fd: evfd, events: POLLIN | POLLERR | POLLHUP };
       ret = conet::co_poll(&pf, 1, -1);

       cnt = 0;
       ret = syscall(SYS_read, evfd, &cnt, sizeof(cnt)); 
       if (tw->stop_flag) break;
       if (ret != sizeof(cnt)) {
          LOG(ERROR)<<" timewheel eventfd read failed, "
               "[ret:"<<ret<<"]"
               "[timerfd:"<<evfd<<"]"
               "[poll event:"<<pf.revents<<"]"
               "[errno:"<<errno<<"]"
               "[errmsg:"<<strerror(errno)<<"]"
               ;
           continue;
       }

       tw->now_ms = get_cur_ms();
       cnt = tw->check(tw->now_ms);
       tw->notify->latest_ms = 0; //get_latest_ms(tw);
    }

    time_mgr_t::instance().free(tw->notify);
    return 0;
}

int timewheel_t::start()
{
    // 开始 定时调度
    coroutine_t *co =  NULL;
    co  = alloc_coroutine(timewheel_task, this, 128*4096);
    this->co = co;
    //conet::set_auto_delete(co);
    conet::resume(co, this);
    return 0;
}

int timewheel_t::stop(int ms)
{
    this->stop_flag = 1;
    coroutine_t *co = (coroutine_t *)this->co; 
    this->co = NULL;
    if (co) {
        conet::resume(co);
        conet::free_coroutine(co);
    }
    return 0;
}

inline
timewheel_t *get_timewheel()
{
    return get_coroutine_env()->tw;
}


void cancel_timeout(timeout_handle_t *obj) {
    if (!list_empty(&obj->link_to)) 
    {

        list_del_init(&obj->link_to);
        timewheel_t * tw = obj->tw;
        if (tw && obj->timeout >0)  {
            --tw->task_num;
        }
    }
    obj->tw = NULL;
}

bool set_timeout_impl(timewheel_t *tw, timeout_handle_t * obj, int timeout, int interval)
{
    list_del_init(&obj->link_to);
    obj->interval = interval;
    obj->tw = tw;
    if (timeout <=0 && interval == 0)
    {
       list_add_tail(&obj->link_to, &tw->now_list);
       tw->co_env->dispatch->delay(&tw->delay_task);
       return true;
    }
    uint64_t cur_ms = get_cur_ms();
    uint64_t t = cur_ms + timeout;
    if (t < tw->prev_ms)  t = tw->prev_ms;
    obj->timeout = t;

    /*
    if (tw->enable_notify) {
        tw->notify->latest_ms = t;
    }
    */

    ++tw->task_num;

    int pos = obj->timeout % tw->slot_num;
    list_add_tail(&obj->link_to, &tw->slots[pos]);
    return true;
}

bool timewheel_t::set_timeout(timeout_handle_t * obj, int timeout)
{
    return set_timeout_impl(this, obj, timeout, 0);
}

bool timewheel_t::set_interval(timeout_handle_t * obj, int interval)
{
    return set_timeout_impl(this, obj, interval, interval);
}

uint64_t get_latest_ms(timewheel_t *tw)
{
    uint64_t now_ms = tw->now_ms;
    int pos = tw->pos;
    int slot_num = tw->slot_num;
    list_head *slots = tw->slots;
    uint64_t min = now_ms + 10;
    for(int i=0; i<10; ++i)
    {
        pos = (pos +i) %slot_num;
        if (!list_empty(&slots[pos])) {
            min  = now_ms + i;
            break;
        }
    }

    return min;
}


int timewheel_t::check(uint64_t cur_ms)
{
    timewheel_t *tw = this;

    if (cur_ms == 0 ) {
        cur_ms = get_cur_ms();
    }

    if (tw->task_num <=0) {
        tw->pos = cur_ms % tw->slot_num;;
        tw->prev_ms = cur_ms;
        return 0;
    }

    int64_t elasp_ms = time_diff(cur_ms, tw->prev_ms);

    int cnt = 0;

    int end_pos = 0;

    if (elasp_ms <=0) return 0;

    int slot_num = tw->slot_num;
    list_head *slots = tw->slots;
    int pos = tw->pos;

    if ((int64_t) elasp_ms >= slot_num)
    {
        end_pos = (pos + slot_num -1) % slot_num;
    } else {
        end_pos = cur_ms % slot_num;
    }

    do
    {
        timeout_handle_t *t1=NULL, *next=NULL;
        list_for_each_entry_safe(t1, next, slots+pos, link_to)
        {
            if (time_after_eq(cur_ms, t1->timeout)) {
                cancel_timeout(t1);
                if (t1->interval) {
                    set_timeout_impl(tw, t1, t1->interval, t1->interval);
                }
                t1->fn(t1->arg);
                ++cnt;
            }
        }
        if (pos == end_pos) break;
        pos = (pos+1) % slot_num;
    } while(1);

    tw->pos = cur_ms % slot_num;;
    tw->prev_ms = cur_ms;
    return cnt;
}

void set_timeout(timeout_handle_t *obj, int timeout /* ms*/)
{
    timewheel_t *tw = get_timewheel();
    tw->set_timeout(obj, timeout);
}


void set_interval(timeout_handle_t *obj, int timeout /* ms*/)
{
    timewheel_t *tw = get_timewheel();
    tw->set_interval(obj, timeout);
}

}
