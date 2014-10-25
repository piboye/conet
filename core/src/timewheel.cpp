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

#include "hook_helper.h"
#include "timewheel.h"
#include "log.h"
#include "dispatch.h"
#include "conet_all.h"
#include "fd_ctx.h"
#include "thirdparty/gflags/gflags.h"
#include "coroutine.h"
#include "coroutine_impl.h"

#include "base/incl/tls.h"
#include "base/incl/time_helper.h"

namespace conet
{
 epoll_ctx_t * get_epoll_ctx();
}

static __thread timewheel_t * g_tw = NULL;

using namespace conet;


DEFINE_int32(timewheel_slot_num, 60*1000, "default timewheel slot num");


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

void init_timeout_handle(timeout_handle_t * self,
               void (*fn)(void *), void *arg, int timeout)
{
    INIT_LIST_HEAD(&self->link_to);
    self->timeout = timeout;
    self->fn = fn;
    self->arg = arg;
    self->tw = NULL;
    self->interval = 0;
}

static 
inline
uint64_t get_cur_ms(timewheel_t *tw)
{
    if (tw && tw->co && tw->update_timeofday_flag)
    {
        return tw->prev_tv.tv_sec*1000UL + tw->prev_tv.tv_usec/1000;
    }
    return get_sys_ms();
}


void init_timewheel(timewheel_t *self, int slot_num)
{
    assert(slot_num > 0);
    list_head *slots = (list_head *) malloc(sizeof(list_head) * slot_num);
    for(int i=0; i<slot_num; ++i) {
        INIT_LIST_HEAD(&slots[i]);
    }
    self->slots = slots;
    self->slot_num = slot_num;
    self->task_num = 0;

    uint64_t cur_ms = get_sys_ms();
    self->pos = cur_ms % slot_num;
    self->prev_ms = cur_ms;
    self->stop = 0;
    self->co = NULL;
    self->timerfd = -1;
    self->update_timeofday_flag = 0;
    self->prev_tv.tv_sec = 0;
    self->prev_tv.tv_usec = 0;
}

void fini_timewheel(timewheel_t *self)
{
    free(self->slots);
}

int check_timewheel(timewheel_t *tw, uint64_t cur_ms);

int check_timewheel(void * arg)
{
    return check_timewheel((timewheel_t *) arg, 0);
}





static
int timewheel_task(void *arg)
{
    conet::enable_sys_hook(); 
    timewheel_t *tw = (timewheel_t *)arg;
    int timerfd = 0;
    timerfd = timerfd_create(CLOCK_MONOTONIC, TFD_NONBLOCK);
    if (timerfd < 0) {
        LOG_SYS_CALL(timerfd_create, timerfd); 
        return -1;
    }
    int ret = 0; 
    /*
    struct timespec now;  
    ret =  clock_gettime(CLOCK_REALTIME, &now);
    if (ret < 0) {
        LOG_SYS_CALL(clock_gettime, ret); 
        return -2;
    }  
    */

    struct itimerspec ts;
    ts.it_value.tv_sec = 0;//now.tv_sec; 
    ts.it_value.tv_nsec = 1000000;//((now.tv_nsec/1000)+1)*1000;
    ts.it_interval.tv_sec = 0;
    ts.it_interval.tv_nsec = 1000000;

    ret = timerfd_settime(timerfd, 0, &ts, NULL);
    if (ret < 0) {
        LOG_SYS_CALL(timerfd_settime, ret);
        return -3;
    }

    coroutine_t * co_self = CO_SELF();

    fd_ctx_t *fd_ctx = NULL;
    //fd_ctx = conet::alloc_fd_ctx(timerfd, fd_ctx_t::TIMER_FD_TYPE);
    fd_ctx = conet::alloc_fd_ctx(timerfd, fd_ctx_t::SOCKET_FD_TYPE);
    
    tw->timerfd = timerfd;

    uint64_t cnt = 0;

    ret = _(gettimeofday)(&tw->prev_tv, NULL);
    tw->update_timeofday_flag = 1;

    //epoll_event ev;
    //ev.events = EPOLLIN| EPOLLERR| EPOLLHUP | EPOLLET;

    //ev.data.ptr = fd_ctx;
    //epoll_ctl(conet::get_epoll_ctx()->m_epoll_fd, EPOLL_CTL_ADD, timerfd,  &ev);
    while (!tw->stop) {

       //fd_ctx->poll_wait_queue.prev = (list_head *)(1);
       //fd_ctx->poll_wait_queue.next = (list_head *)(co_self);
       //conet::yield(NULL, NULL);

       struct pollfd pf = { fd: timerfd, events: POLLIN | POLLERR | POLLHUP };
       ret = poll(&pf, 1, -1);
       
       ret = syscall(SYS_read, timerfd, &cnt, sizeof(cnt)); 
       if (ret != sizeof(cnt)) {
           LOG(ERROR)<<" timewheel read failed";
           continue;
       }

       ret = _(gettimeofday)(&tw->prev_tv, NULL);

       check_timewheel(tw, get_cur_ms(tw));
    }
    //epoll_ctl(get_epoll_ctx()->m_epoll_fd, EPOLL_CTL_DEL, timerfd,  &ev);
    //LOG(INFO)<<"timewheel stop"; 
    return 0;
}

void stop_timewheel(timewheel_t *self)
{
    self->stop = 1;
    conet::wait((coroutine_t *)self->co);
}


timewheel_t *alloc_timewheel()
{
    timewheel_t *tw = (timewheel_t *) malloc(sizeof(timewheel_t));
    init_timewheel(tw, FLAGS_timewheel_slot_num);
    coroutine_t *co = alloc_coroutine(timewheel_task, tw);
    tw->co = co;
    resume(co);
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
    free(tw);
}


void cancel_timeout(timeout_handle_t *obj) {
    list_del_init(&obj->link_to);
    timewheel_t * tw = obj->tw;
    if (tw)  {
        --tw->task_num;
    }
    obj->tw = NULL;
}

bool set_timeout_impl(timewheel_t *tw, timeout_handle_t * obj, int timeout, int interval)
{
    assert(list_empty(&obj->link_to));
    assert (timeout >=0);
    if (timeout <0) timeout = 0;
    uint64_t cur_ms = get_cur_ms(tw);
    uint64_t t = cur_ms + timeout;
    if (t < tw->prev_ms)  t = tw->prev_ms;
    obj->timeout = t;

    ++tw->task_num;

    int pos = obj->timeout % tw->slot_num;
    list_add(&obj->link_to, &tw->slots[pos]);
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
                    set_timeout_impl(t1->tw, t1, t1->interval, t1->interval);
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


static
DEF_TLS_GET(g_tw, alloc_timewheel(),  free_timewheel)

void set_timeout(timeout_handle_t *obj, int timeout /* ms*/)
{
    set_timeout(tls_get(g_tw), obj, timeout);
}


void set_interval(timeout_handle_t *obj, int timeout /* ms*/)
{
    set_interval(tls_get(g_tw), obj, timeout);
}
