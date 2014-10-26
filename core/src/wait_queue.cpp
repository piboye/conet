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
#include "core/incl/wait_queue.h"
#include "core/incl/coroutine.h"

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

}

