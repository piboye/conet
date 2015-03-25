/*
 * =====================================================================================
 *
 *       Filename:  time_mgr.h
 *
 *    Description:
 *
 *        Version:  1.0
 *        Created:  03/24/2015 10:08:45 PM
 *       Revision:  none
 *       Compiler:  gcc
 *
 *         Author:  piboye
 *   Organization:
 *
 * =====================================================================================
 */

#ifndef __TIME_MGR_H_INC__
#define __TIME_MGR_H_INC__
#include "base/obj_pool.h"
#include <pthread.h>
#include "base/list.h"
#include <stdint.h>

namespace conet
{

struct timeout_notify_t
{
    list_head link_to;
    uint64_t latest_ms; // 最近的超时时间
    int event_fd;
    int status; // 1 准备入队列, 2 已经入队列, 3准备离队列, 4 离队成功
    timeout_notify_t()
    {
        init();
    }
    void init ()
    {
        latest_ms = 0;
        event_fd = -1;
        status = 0;
        INIT_LIST_HEAD(&link_to);
    }
};

struct time_mgr_t 
{
    int32_t stop_flag;
    int32_t time_resolution;
    int timerfd;
    pthread_t *tid;
    pthread_mutex_t this_mutex;
    timeval * gettimeofday_cache;
    uint64_t cur_ms;
    
    ObjPool<timeout_notify_t> notify_pool;

    list_head timeout_notify_queue; // 超时通知队列
    uint64_t in_queue_num;
    list_head timeout_notify_inqueue; // 入队队列
    list_head timeout_notify_dequeue; // 出队队列

    time_mgr_t()
    {
        timerfd = 0;
        stop_flag = 0;
        time_resolution = 1;
        tid = NULL;
        in_queue_num = 0;
        cur_ms = 0;
        pthread_mutex_init(&this_mutex, NULL);
        INIT_LIST_HEAD(&timeout_notify_queue);
        INIT_LIST_HEAD(&timeout_notify_inqueue);
        INIT_LIST_HEAD(&timeout_notify_dequeue);
    }

    static time_mgr_t &instance();


    timeout_notify_t *alloc_timeout_notify();

    void free(timeout_notify_t *);

    void add_to_queue(timeout_notify_t *);

    int start();
    int stop();
    

    //private
    timeval tvs[100];
    int tvs_pos;

    void * main_proc();
    void update_gettimeofday_cache();
    void do_in_queue();
    void check_timeout();
    void do_dequeue();
};

}

#endif /* end of include guard */
