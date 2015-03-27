/*
 * =====================================================================================
 *
 *       Filename:  timewheel.cpp
 *
 *    Description:
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
#include "coroutine_impl.h"

#include "base/tls.h"
#include "base/time_helper.h"

namespace conet
{

static __thread timewheel_t * g_tw = NULL;
epoll_ctx_t * get_epoll_ctx();

static inline uint64_t get_cur_ms()
{
    return time_mgr_t::instance().cur_ms;
}

uint64_t get_tick_ms()
{
    return time_mgr_t::instance().cur_ms;
}

uint64_t get_sys_ms()
{
    return time_mgr_t::instance().cur_ms;
}

using namespace conet;

bool set_timeout_impl(timewheel_t *tw, timeout_handle_t * obj, int timeout, int interval);

DEFINE_int32(timewheel_slot_num, 60*1000, "default timewheel slot num");


void init_timeout_handle(timeout_handle_t * self, void (*fn)(void *), void *arg)
{
    INIT_LIST_HEAD(&self->link_to);
    self->timeout = 0;
    self->fn = fn;
    self->arg = arg;
    self->tw = NULL;
    self->interval = 0;
}


int check_timewheel(void * arg)
{
    return check_timewheel((timewheel_t *) arg, 0);
}

static
int do_now_task(void *arg)
{
    timewheel_t *tw = (timewheel_t *)(arg);
    timeout_handle_t *it=NULL, *next=NULL;
    list_for_each_entry_safe(it, next, &tw->now_list, link_to)
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

void init_timewheel(timewheel_t *self, int slot_num)
{
    assert(slot_num > 0);
    list_head *slots = new list_head[slot_num];
    for(int i=0; i<slot_num; ++i) {
        INIT_LIST_HEAD(&slots[i]);
    }
    self->slots = slots;
    self->slot_num = slot_num;
    self->task_num = 0;

    uint64_t cur_ms = get_cur_ms();
    self->now_ms = cur_ms;
    self->pos = cur_ms % slot_num;
    self->prev_ms = cur_ms;
    self->stop = 0;
    self->co = NULL;
    self->notify = time_mgr_t::instance().alloc_timeout_notify();
    self->enable_notify = 1;

    INIT_LIST_HEAD(&self->now_list);
    init_task(&self->delay_task, &do_now_task, self);
    registry_task(&self->delay_task);
}

void fini_timewheel(timewheel_t *self)
{
    unregistry_task(&self->delay_task);
    delete [] self->slots;
}

int check_timewheel(timewheel_t *tw, uint64_t cur_ms);


uint64_t get_latest_ms(timewheel_t *tw);

static int timewheel_task(void *arg)
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
    while (!tw->stop)
    {
       struct pollfd pf = { fd: evfd, events: POLLIN | POLLERR | POLLHUP };
       ret = conet::co_poll(&pf, 1, -1);

       cnt = 0;
       ret = syscall(SYS_read, evfd, &cnt, sizeof(cnt)); 
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
       cnt = check_timewheel(tw, tw->now_ms);
       tw->notify->latest_ms = get_latest_ms(tw);
    }

    time_mgr_t::instance().free(tw->notify);
    return 0;
}

void stop_timewheel(timewheel_t *self)
{
    self->stop = 1;
    conet::wait((coroutine_t *)self->co);
}

timewheel_t *alloc_timewheel()
{
    timewheel_t *tw =  new timewheel_t();
    init_timewheel(tw, FLAGS_timewheel_slot_num);
    return tw;
}

void free_timewheel(timewheel_t *tw)
{
    fini_timewheel(tw);
    if (tw->co) {
        if (conet::is_stop((coroutine_t *)tw->co))  {
            tw->stop = 1;
            conet::wait((coroutine_t *)tw->co);
        }
        free_coroutine((coroutine_t *)tw->co);
    }
    delete tw;
}

timewheel_t *get_timewheel()
{
    if (NULL == g_tw) {
        timewheel_t *tw = alloc_timewheel();
        if (NULL == g_tw) {
            g_tw = tw;
            coroutine_t *co =  NULL;
            co  = alloc_coroutine(timewheel_task, tw, 128*4096);
            tw->co = co;
            // 这个必须在 alloc_coroutine 下面
            tls_onexit_add(tw, (void (*)(void *))&free_timewheel);
            conet::resume((coroutine_t *)tw->co);
        } else {
            free_timewheel(tw);
        }
    }
    return g_tw;
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
       return true;
    }
    uint64_t cur_ms = get_cur_ms();
    uint64_t t = cur_ms + timeout;
    if (t < tw->prev_ms)  t = tw->prev_ms;
    obj->timeout = t;

    if (tw->enable_notify) {
        tw->notify->latest_ms = t;
    }

    ++tw->task_num;

    int pos = obj->timeout % tw->slot_num;
    list_add_tail(&obj->link_to, &tw->slots[pos]);
    return true;
}

bool set_timeout(timewheel_t *tw, timeout_handle_t * obj, int timeout)
{
    return set_timeout_impl(tw, obj, timeout, 0);
}

bool set_interval(timewheel_t *tw, timeout_handle_t * obj, int interval)
{
    return set_timeout_impl(tw, obj, interval, interval);
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


int check_timewheel(timewheel_t *tw, uint64_t cur_ms)
{

    if (cur_ms == 0 ) {
        cur_ms = get_cur_ms();
    }

    /*
    if (tw->task_num <=0) {
        tw->pos = cur_ms % tw->slot_num;;
        tw->prev_ms = cur_ms;
        return 0;
    }
    */

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
    set_timeout(tw, obj, timeout);
}


void set_interval(timeout_handle_t *obj, int timeout /* ms*/)
{
    timewheel_t *tw = get_timewheel();
    set_interval(tw, obj, timeout);
}

}
