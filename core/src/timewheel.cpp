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

void init_timeout_handle(timeout_handle_t * self, 
        void (*fn)(void *), void *arg, int timeout)
{
		INIT_LIST_HEAD(&self->link_to);
		self->timeout = timeout;
		self->fn = fn;
		self->arg = arg;
        self->tw = NULL;
}

#define get_cur_ms get_sys_ms

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

timewheel_t *alloc_timewheel()
{
    timewheel_t *tw = (timewheel_t *) malloc(sizeof(timewheel_t));
    init_timewheel(tw, 60 * 1000);
    conet::registry_task(&check_timewheel, tw);
    return tw;
}

void free_timewheel(timewheel_t *tw)
{
    fini_timewheel(tw);
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

bool set_timeout(timewheel_t *tw, timeout_handle_t * obj, int timeout) 
{
    assert(list_empty(&obj->link_to));
    assert (timeout >=0);
    if (timeout <0) return false;
    uint64_t cur_ms = get_cur_ms();
    uint64_t t = cur_ms + timeout;
    if (t < tw->prev_ms) return false;
    obj->timeout = t;

    ++tw->task_num;

    int pos = obj->timeout % tw->slot_num;
    list_add(&obj->link_to, &tw->slots[pos]);
    obj->tw = tw;
    //CONET_LOG(INFO, "obj:%p, tw:%p", obj, tw);
    return true;
}



int check_timewheel(timewheel_t *tw, uint64_t cur_ms)
{
    if (cur_ms == 0 ) {
        cur_ms = get_cur_ms();
    }
    int64_t elasp_ms = time_diff(cur_ms, tw->prev_ms);
    assert(elasp_ms >=0);

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
                //timeout and call timeout callback;
               
                //CONET_LOG(DEBUG, "timeout, obj:%p, cur:%lu, timeout:%lu", t1, cur_ms, t1->timeout);
                cancel_timeout(t1);
                // cancel_timeout must call before t1-fn callbak
                // because fn callback would coroutine swap
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
