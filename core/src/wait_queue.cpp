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

namespace 
{

struct WaitItem
{
    conet::WaitQueue * wq;
    conet::coroutine_t *co;
    list_head link;
    conet::timeout_handle_t tm;
    int expired_flag;

};

void wait_queue_item_timeout(void *arg)
{
    WaitItem *self = (WaitItem *)(arg);
    self->expired_flag = 1;
    resume(self->co, NULL);
}

}

namespace conet
{


WaitQueue::WaitQueue()
{
    this->wait_num = 0;
    INIT_LIST_HEAD(&this->queue);
}

WaitQueue::~WaitQueue()
{
    WaitItem *it=NULL, *next = NULL;

    list_for_each_entry_safe(it, next, &queue, link)
    {
        list_del(&it->link);
        conet::resume(it->co);
    }
}

int WaitQueue::wait_on(int ms)
{
     WaitItem w;
     coroutine_t * co = CO_SELF(); 

     // 初始化 wait_item
     w.wq = this;
     w.co = co; 
     w.expired_flag = 0;
     INIT_LIST_HEAD(&w.link);

     if (ms >= 0) {
        init_timeout_handle(&w.tm, wait_queue_item_timeout, &w);
        set_timeout(&w.tm, ms);
     }

     list_add_tail(&w.link, &this->queue);

     ++this->wait_num;

     yield(NULL, NULL);


     --this->wait_num; 

     if (ms >=0) {
        cancel_timeout(&w.tm);
     }

     if (w.expired_flag) {
         //timeout
         return -1;          
     }

     return 0;
}

int WaitQueue::wakeup_head_n(int num)
{
    int n = 0;

    WaitItem *it=NULL, *next=NULL;
    list_for_each_entry_safe(it, next, &this->queue, link)
    {
        list_del(&it->link);
        coroutine_t *co = it->co;
        conet::resume(co);
        ++n;
        if (n >= num) break;
    }
    return n;
}

int WaitQueue::wakeup_head()
{
    if (!list_empty(&this->queue)) {
        WaitItem *item = container_of(this->queue.next, WaitItem, link); 
        list_del(&item->link);
        coroutine_t *co = item->co;
        conet::resume(co);
        return 1;
    }
    return 0;
}

int WaitQueue::wakeup_tail()
{
    if (!list_empty(&this->queue)) {
        WaitItem *item = container_of(this->queue.prev, WaitItem, link); 
        list_del(&item->link);
        coroutine_t *co = item->co;
        conet::resume(co);
        return 1;
    }
    return 0;
}

int WaitQueue::wakeup_tail_n(int num)
{
    int n = 0;

    WaitItem *it=NULL, *next=NULL;
    list_for_each_entry_safe_reverse(it, next, &this->queue, link)
    {
        list_del(&it->link);
        coroutine_t *co = it->co;
        conet::resume(co);
        ++n;
        if (n >= num) break;
    }
    return n;
}

int WaitQueue::wakeup_all()
{
    int n = 0;

    WaitItem *it=NULL, *next=NULL;
    list_for_each_entry_safe(it, next, &this->queue, link)
    {
        list_del(&it->link);
        coroutine_t *co = it->co;
        conet::resume(co);
        ++n;
    }
    return n;
}

static
void timeout_proc(void *arg)
{
    CondWaitQueue *self = (CondWaitQueue *)arg;
    self->wait_queue.wakeup_all();
    return;
}

CondWaitQueue::CondWaitQueue()
{
    delay_ms = 0;
    func_arg = NULL;
    cond_check_func = NULL;
    init_timeout_handle(&tm, &timeout_proc, this);
}

int CondWaitQueue::init(int (*func)(void *arg), void *arg, int delay_ms)
{
    cond_check_func = func;
    func_arg = arg;
    this->delay_ms = delay_ms;
    return 0;
}

int CondWaitQueue::wait_on(int times)
{
    return this->wait_queue.wait_on(times);
}

int CondWaitQueue::wakeup_all()
{
    
    int ret = 0;

    if (cond_check_func) {
        ret = cond_check_func(func_arg);
        if (ret >0) {
            cancel_timeout(&tm);
            return this->wait_queue.wakeup_all();
        } else {
            if (list_empty(&tm.link_to)) {
                set_timeout(&tm, delay_ms); 
            }
        }
    } else {
        return this->wait_queue.wakeup_all();
    }

    return 0;
}

}

