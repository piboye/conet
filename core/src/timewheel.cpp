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
 *         Author:  piboyeliu
 *   Organization:
 *
 * =====================================================================================
 */
#include <stdlib.h>
#include "timewheel.h"
#include <assert.h>
#include <stdio.h>
#include "log.h"
#include "tls.h"
#include "dispatch.h"
#include "time_helper.h"
#include "conet_all.h"
#include <sys/timerfd.h>  

using namespace conet;

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

#define get_cur_ms conet::get_cached_ms


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

    uint64_t cur_ms = get_cur_ms();
    self->pos = cur_ms % slot_num;
    self->prev_ms = cur_ms;
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

#define LOG_SYS_CALL(func, ret) \
        LOG(ERROR)<<"syscall "<<#func <<" failed, [ret:"<<ret<<"]" \
                    "[errno:"<<errno<<"]" \
                    "[errmsg:"<<strerror(errno)<<"]" \
                    ; \

int timewheel_task(void *arg)
{
    conet::enable_sys_hook(); 
    //LOG(INFO)<<" timewheel start";
    timewheel_t *tw = (timewheel_t *)arg;
    int timerfd = 0;
    timerfd = timerfd_create(CLOCK_MONOTONIC, TFD_NONBLOCK);
    if (timerfd < 0) {
        LOG_SYS_CALL(timerfd_create, timerfd); 
        return -1;
    }
    int ret = 0; 
    struct timespec now;  
    ret =  clock_gettime(CLOCK_REALTIME, &now);
    if (ret < 0) {
        LOG_SYS_CALL(clock_gettime, ret); 
        return -2;
    }

    struct itimerspec ts;
    ts.it_value.tv_sec = now.tv_sec; 
    ts.it_value.tv_nsec = now.tv_nsec+1000;
    ts.it_interval.tv_sec = 0;
    ts.it_interval.tv_nsec = 1000;

    ret = timerfd_settime(timerfd, 0, &ts, NULL);
    if (ret < 0) {
        LOG_SYS_CALL(timerfd_settime, ret);
        return -3;
    }
    
    tw->timerfd = timerfd;
    uint64_t cnt = 0;
    while (1) {
       struct pollfd pf = {
            fd: timerfd,
            events: POLLIN | POLLERR | POLLHUP
       };
       ret = poll(&pf, 1, -1);
       if (ret == 0) {
            break;
       }
       if (pf.revents & POLLERR) {
           break;
       }
       ret = read(timerfd, &cnt, sizeof(cnt)); 
       if (ret != sizeof(cnt)) {
           LOG(ERROR)<<" timewheel read failed";
           continue;
       }
       check_timewheel(tw);
    }
    //LOG(INFO)<<" timewheel stop";
    return 0;
}


timewheel_t *alloc_timewheel()
{
    timewheel_t *tw = (timewheel_t *) malloc(sizeof(timewheel_t));
    init_timewheel(tw, 60 * 1000);
    //conet::registry_task(&check_timewheel, tw);
    coroutine_t *co = alloc_coroutine(timewheel_task, tw);
    tw->extend = co;
    resume(co);
    return tw;
}

void free_timewheel(timewheel_t *tw)
{
    fini_timewheel(tw);
    if (tw->extend) {
        free_coroutine((coroutine_t *)tw->extend);
    }
    free(tw);
}


void cancel_timeout(timeout_handle_t *obj) {
    //assert(!list_empty(&obj->link_to));
    list_del_init(&obj->link_to);
    timewheel_t * tw = obj->tw;
    if (tw)  {
        --tw->task_num;
    }
    //CONET_LOG(INFO, "obj:%p, tw:%p", obj, tw);
    obj->tw = NULL;
}

bool set_timeout_impl(timewheel_t *tw, timeout_handle_t * obj, int timeout, int interval)
{
    assert(list_empty(&obj->link_to));
    assert (timeout >=0);
    if (timeout <0) timeout = 0;
    uint64_t cur_ms = get_cur_ms();
    uint64_t t = cur_ms + timeout;
    if (t < tw->prev_ms)  t = tw->prev_ms;
    obj->timeout = t;

    ++tw->task_num;

    int pos = obj->timeout % tw->slot_num;
    list_add(&obj->link_to, &tw->slots[pos]);
    obj->tw = tw;
    obj->interval = interval;
    //CONET_LOG(INFO, "obj:%p, tw:%p", obj, tw);
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
        cur_ms = get_cur_ms();
    }

    int64_t elasp_ms = time_diff(cur_ms, tw->prev_ms);

    // this is import, can speed up 30%
    if (elasp_ms <=0) return 0;

    int cnt = 0;

    int end_pos = 0;

    int slot_num = tw->slot_num;
    list_head *slots = tw->slots;
    int pos = tw->pos;

    if ((int64_t) elasp_ms >= slot_num)
    {
        end_pos = (pos + slot_num -1) % slot_num;
    } else {
        end_pos = cur_ms % slot_num;
    }

    int pcnt= 0;
    do
    {
        ++pcnt;
        list_head *it=NULL, *next=NULL;
        list_for_each_safe(it, next, slots+pos)
        {
            timeout_handle_t *t1 = container_of(it, timeout_handle_t, link_to);
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
    //CONET_LOG(DEBUG, "timewheel procc slots:%d", pcnt);

    tw->pos = cur_ms % slot_num;;
    tw->prev_ms = cur_ms;
    return cnt;
}

static
__thread timewheel_t * g_tw = NULL;

static
DEF_TLS_GET(g_tw, alloc_timewheel(),  free_timewheel)

void set_timeout(timeout_handle_t *obj, int timeout /* ms*/)
{
    set_timeout(tls_get(g_tw), obj, timeout);
}
