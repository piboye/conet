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

#include "gflags/gflags.h"
#include "hook_helper.h"
#include "timewheel.h"
#include "log.h"
#include "dispatch.h"
#include "conet_all.h"
#include "fd_ctx.h"
#include "coroutine.h"
#include "coroutine_impl.h"

#include "../../base/tls.h"
#include "../../base/time_helper.h"


namespace conet
{

void close_fd_notify_poll(int fd);

static __thread timewheel_t * g_tw = NULL;
 epoll_ctx_t * get_epoll_ctx();

static uint64_t g_khz = get_cpu_khz();
inline
uint64_t get_tick_ms2() 
{

    return rdtscp() / g_khz;
}

//uint64_t get_tick_ms() __attribute__((strong));
uint64_t get_tick_ms() 
{
    if (g_tw) {
        return g_tw->now_ms;
    }
    return get_tick_ms2();
}


using namespace conet;


DEFINE_int32(timewheel_slot_num, 60*1000, "default timewheel slot num");


/*
HOOK_CPP_FUNC_DEF(int , gettimeofday,(struct timeval *tv, struct timezone *tz))
{
    HOOK_SYS_FUNC(gettimeofday);
    if (tz != NULL || NULL == g_tw) return _(gettimeofday)(tv, tz);

    int ret = 0;
    if (g_tw->update_timeofday_flag == 0) {
         ret = _(gettimeofday)(tv, NULL);
         return ret;
    }

    tv->tv_sec = g_tw->prev_tv.tv_sec;
    tv->tv_usec = g_tw->prev_tv.tv_usec;
    return ret;
}
*/

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
inline
uint64_t get_cur_ms(timewheel_t *tw)
{
    if (tw) {
        return tw->now_ms;
    }
    return get_tick_ms2();
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
       if(0 == it->interval) 
       {
          list_del_init(&it->link_to);
       }

       it->fn(it->arg);
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

    uint64_t cur_ms = get_tick_ms2();
    self->now_ms = cur_ms;
    self->pos = cur_ms % slot_num;
    self->prev_ms = cur_ms;
    self->stop = 0;
    self->co = NULL;
    self->timerfd = -1;
    self->update_timeofday_flag = 0;
    self->prev_tv.tv_sec = 0;
    self->prev_tv.tv_usec = 0;

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


static 
int create_timer_fd()
{
    int timerfd = 0;
    timerfd = timerfd_create(CLOCK_MONOTONIC, TFD_NONBLOCK | TFD_CLOEXEC);
    if (timerfd < 0) {
        LOG_SYS_CALL(timerfd_create, timerfd); 
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
        LOG_SYS_CALL(timerfd_settime, ret);
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

    //ret = _(gettimeofday)(&tw->prev_tv, NULL);
    //tw->update_timeofday_flag = 1;
    tw->now_ms = get_tick_ms2(); 

    while (!tw->stop) {
       struct pollfd pf = { fd: timerfd, events: POLLIN | POLLERR | POLLHUP };
       ret = co_poll(&pf, 1, -1);

       cnt = 0;
       ret = syscall(SYS_read, timerfd, &cnt, sizeof(cnt)); 
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

       //ret = _(gettimeofday)(&tw->prev_tv, NULL);
       
       tw->now_ms = get_tick_ms2(); 
       //tw->now_ms += cnt; 

       check_timewheel(tw, tw->now_ms);
       //registry_delay_task(&tw->delay_task);
    }
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
            coroutine_t *co = alloc_coroutine(timewheel_task, tw, 128*4096);
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
    { // timeout must check empty

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
    if (timeout <=0)
    {
       list_add_tail(&obj->link_to, &tw->now_list);
       return true;
    }

    uint64_t cur_ms = get_cur_ms(tw);
    uint64_t t = cur_ms + timeout;
    if (t < tw->prev_ms)  t = tw->prev_ms;
    obj->timeout = t;

    ++tw->task_num;

    int pos = obj->timeout % tw->slot_num;
    list_add_tail(&obj->link_to, &tw->slots[pos]);
    obj->tw = tw;
    obj->interval = interval;
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



int check_timewheel(timewheel_t *tw, uint64_t cur_ms)
{

    if (cur_ms == 0 ) {
        cur_ms = get_cur_ms(tw);
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
    set_timeout(tw, obj, timeout);
}


void set_interval(timeout_handle_t *obj, int timeout /* ms*/)
{
    timewheel_t *tw = get_timewheel();
    set_interval(tw, obj, timeout);
}
}
