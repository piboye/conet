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


#include "../base/list.h"
#include "timewheel.h"

namespace conet
{

// 等待队列
struct WaitQueue
{
    int wait_num;
    list_head queue;


    WaitQueue();

    ~WaitQueue();

    //  -1 超时, 0 是成功
    int wait_on(int ms = -1);

    int wakeup_all();

    int wakeup_head_n(int num=1);

    int wakeup_head();

    int wakeup_tail_n(int num=1);

    int wakeup_tail();
};

// 条件等待队列
//
struct CondWaitQueue
{
    WaitQueue wait_queue;

    int (*cond_check_func)(void *arg);
    void *func_arg;

    int delay_ms;

    int init(int (*func)(void *arg), void *arg, int delay_ms);

    timeout_handle_t tm; //超时控制

//method
    CondWaitQueue();

    int wait_on(int times=-1);

    int wakeup_all();

};

}

#endif /* end of include guard */
