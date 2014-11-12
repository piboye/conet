/*
 * =====================================================================================
 *
 *       Filename:  wait_queue.cpp
 *
 *    Description:  
 *
 *        Version:  1.0
 *        Created:  2014年09月12日 18时26分09秒
 *       Revision:  none
 *       Compiler:  gcc
 *
 *         Author:  piboye
 *   Organization:  
 *
 * =====================================================================================
 */
#include <stdlib.h>
#include "wait_queue.h"
#include "coroutine.h"

namespace conet
{

void init_wait_queue(wait_queue_t *q)
{
    q->wait_num = 0;
    INIT_LIST_HEAD(&q->queue);
}

void wait_queue_item_timeout(void *arg)
{
    wait_item_t *self = (wait_item_t *)(arg);
    self->expired_flag = 1;
    resume(self->co, NULL);
}

int wait_on(wait_queue_t *q, int ms)
{
     wait_item_t w;
     coroutine_t * co = CO_SELF(); 

     w.wq = q;

     w.co = co; 
     w.delete_self = 0;
     w.expired_flag = 0;
     INIT_LIST_HEAD(&w.link);

     if (ms >= 0) {
        init_timeout_handle(&w.tm, wait_queue_item_timeout, &w, ms);
        set_timeout(&w.tm, ms);
     }

     list_add_tail(&w.link, &q->queue);
     ++q->wait_num;

     yield(NULL, NULL);

     --q->wait_num; 

     if (w.expired_flag) {
         //timeout
         return -1;          
     }

     return 0;
}

int wakeup_head_n(wait_queue_t * q, int times)
{
    int n = 0;

    wait_item_t *it=NULL, *next=NULL;
    list_for_each_entry_safe(it, next, &q->queue, link)
    {
        list_del(&it->link);
        coroutine_t *co = it->co;
        if (it->delete_self) {
            free(it);
        }
        conet::resume(co);
        ++n;
        if (n >= times) break;
    }
    return n;
}

int wakeup_head(wait_queue_t * q)
{
    if (!list_empty(&q->queue)) {
        wait_item_t *item = container_of(q->queue.next, wait_item_t, link); 
        list_del(&item->link);
        coroutine_t *co = item->co;
        if (item->delete_self) {
            free(item);
        }
        conet::resume(co);
        return 1;
    }
    return 0;
}

int wakeup_tail(wait_queue_t * q)
{
    if (!list_empty(&q->queue)) {
        wait_item_t *item = container_of(q->queue.prev, wait_item_t, link); 
        list_del(&item->link);
        coroutine_t *co = item->co;
        if (item->delete_self) {
            free(item);
        }
        conet::resume(co);
        return 1;
    }
    return 0;
}

int wakeup_tail_n(wait_queue_t * q, int times)
{
    int n = 0;

    wait_item_t *it=NULL, *next=NULL;
    list_for_each_entry_safe_reverse(it, next, &q->queue, link)
    {
        list_del(&it->link);
        coroutine_t *co = it->co;
        if (it->delete_self) {
            free(it);
        }
        conet::resume(co);
        ++n;
        if (n >= times) break;
    }
    return n;
}

int wakeup_all(wait_queue_t * q)
{
    int n = 0;

    wait_item_t *it=NULL, *next=NULL;
    list_for_each_entry_safe(it, next, &q->queue, link)
    {
        list_del(&it->link);
        coroutine_t *co = it->co;
        if (it->delete_self) {
            free(it);
        }
        conet::resume(co);
        ++n;
    }
    return n;
}

cond_wait_queue_t::cond_wait_queue_t()
{
    init_wait_queue(&this->wait_queue);
    delay_ms = 0;
    func_arg = NULL;
    cond_func = NULL;
    init_timeout_handle(&tm, &cond_wait_queue_t::timeout_proc, this, -1);
}

void cond_wait_queue_t::timeout_proc(void *arg)
{
    cond_wait_queue_t *self = (cond_wait_queue_t *)arg;
    conet::wakeup_all(&self->wait_queue);
    return;
}

int cond_wait_queue_t::wain_on(int times)
{
    return wait_on(&this->wait_queue, times);
}

int cond_wait_queue_t::wakeup_all()
{
    
    int ret = 0;

    if (cond_func) {
        ret = cond_func(func_arg);
        if (ret >0) {
            cancel_timeout(&tm);
            return conet::wakeup_all(&this->wait_queue);
        } else {
            if (list_empty(&tm.link_to)) {
                set_timeout(&tm, delay_ms); 
            }
        }
    } else {
        return conet::wakeup_all(&this->wait_queue);
    }
    return 0;
}

}

