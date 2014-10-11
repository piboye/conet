/*
 * =====================================================================================
 *
 *       Filename:  wait_queue.h
 *
 *    Description:  
 *
 *        Version:  1.0
 *        Created:  2014年09月12日 15时51分00秒
 *       Revision:  none
 *       Compiler:  gcc
 *
 *         Author:  piboye
 *   Organization:  
 *
 * =====================================================================================
 */
#ifndef WAIT_QUEUE_H_INCL
#define WAIT_QUEUE_H_INCL


#include "base/incl/list.h"

namespace conet
{

struct coroutine_t;

struct wait_item_t
{
    coroutine_t *co;
    list_head link;
    int delete_self;
};

struct
wait_queue_t
{
    list_head queue;
};

int wakeup_one(wait_queue_t *);

int wakeup_all(wait_queue_t *);

int wakeup_tail(wait_queue_t *);

}

#endif /* end of include guard */
