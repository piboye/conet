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

int wakeup_one(wait_queue_t * q)
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


