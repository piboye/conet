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
#include "timewheel.h"

namespace conet
{

struct coroutine_t;

struct wait_queue_t;

struct wait_item_t
{
    wait_queue_t * wq;
    coroutine_t *co;
    list_head link;
    timeout_handle_t tm;
    int delete_self;
    int expired_flag;
};

struct
wait_queue_t
{
    int wait_num;
    list_head queue;
};

int wait_on(wait_queue_t *q, int ms = -1);

void init_wait_queue(wait_queue_t *q);

int wakeup_all(wait_queue_t *);

int wakeup_head_n(wait_queue_t *, int times=1);

int wakeup_head(wait_queue_t *);

int wakeup_tail_n(wait_queue_t *, int times=1);

int wakeup_tail(wait_queue_t *);

}

#endif /* end of include guard */
