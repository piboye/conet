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
    uint64_t __attribute__((aligned(8))) latest_ms; // 最近的超时时间
    int __attribute__((aligned(8))) status; // 1 准备入队列, 2 已经入队列, 3准备离队列, 4 离队成功
    int event_fd;

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
    int32_t time_resolution;
    int timerfd;
    pthread_t *tid;
    pthread_mutex_t this_mutex;

    // 64bit, 8字节对齐， 一个写， 多个读， 安全
    timeval * __attribute__((aligned(8))) gettimeofday_cache;
    uint64_t __attribute__((aligned(8))) cur_ms;
    uint64_t __attribute__((aligned(8))) in_queue_num;
    int64_t __attribute__((aligned(8))) stop_flag;
 
    ObjPool<timeout_notify_t> notify_pool;

    list_head timeout_notify_queue; // 超时通知队列


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

extern time_mgr_t g_time_mgr;

inline
time_mgr_t & time_mgr_t::instance()
{
    return g_time_mgr;
}
}

#endif /* end of include guard */
