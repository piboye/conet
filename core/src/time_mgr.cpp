/*
 * =====================================================================================
 *
 *       Filename:  time_mgr.cpp
 *
 *    Description:  
 *
 *        Version:  1.0
 *        Created:  03/24/2015 07:28:25 PM
 *       Revision:  none
 *       Compiler:  gcc
 *
 *         Author:  piboye
 *   Organization:  
 *
 * =====================================================================================
 */
#include <stdlib.h>
#include "glog/logging.h"
#include "base/defer.h"
#include <sys/timerfd.h>
#include <sys/syscall.h>

#include "gflags/gflags.h"
#include <pthread.h>
#include "time_mgr.h"
#include <sys/time.h>
#include "hook_helper.h"
#include "base/ptr_cast.h"
#include "coroutine.h"

HOOK_DECLARE( int , gettimeofday, (struct timeval *tv, struct timezone *tz));
HOOK_DECLARE( int , read, (int fd, void *buf, size_t len));

HOOK_DECLARE(int, pthread_mutex_lock,(pthread_mutex_t *mutex));

namespace conet
{
    DEFINE_int32(time_resolution, 1, "set time resolution by micosencond");

time_mgr_t g_time_mgr;

class ScopeLock
{
public:
    pthread_mutex_t & mutex;
    ScopeLock(pthread_mutex_t & m): mutex(m) 
    {
        pthread_mutex_lock(&mutex);
    }
    ~ScopeLock()
    {
        pthread_mutex_unlock(&mutex);
    }
};

#define SCOPE_LOCK(mutex) ScopeLock __lock__##__LINE__(mutex)

timeout_notify_t *time_mgr_t::alloc_timeout_notify()
{
    SCOPE_LOCK(this_mutex);
    timeout_notify_t * t= this->notify_pool.alloc();
    t->init();
    return t;
}

void time_mgr_t::free(timeout_notify_t *t)
{
    // 准备离队
    t->status = 3;
}

void time_mgr_t::add_to_queue(timeout_notify_t *t)
{
    t->status = 1;
    SCOPE_LOCK(this_mutex);
    list_del_init(&t->link_to);
    list_add_tail(&t->link_to, &timeout_notify_inqueue);
    ++in_queue_num;
}

static 
int create_timer_fd()
{
    int timerfd = -1;
    timerfd = timerfd_create(CLOCK_MONOTONIC,  TFD_NONBLOCK | TFD_CLOEXEC);
    if (timerfd < 0) {
        LOG(ERROR)<<"timerfd_create failed, "
            "[ret:"<<timerfd<<"]"
            "[errno:"<<errno<<"]"
            "[errmsg:"<<strerror(errno)<<"]"
            ;
        return -1;
    }
    int ret = 0; 

    struct itimerspec ts;
    ts.it_value.tv_sec = 0;       //
    ts.it_value.tv_nsec = 1000000; // 1ms
    ts.it_interval.tv_sec = 0;
    ts.it_interval.tv_nsec = 1000000; //1ms

    ret = timerfd_settime(timerfd, 0, &ts, NULL);
    if (ret < 0) {
        LOG(ERROR)<<"timerfd_settime failed, "
            "[ret:"<<ret<<"]"
            "[errno:"<<errno<<"]"
            "[errmsg:"<<strerror(errno)<<"]"
            ;
        return -3;
    }

    return timerfd;
}

void * time_mgr_t::main_proc()
{
    memset(tvs, 0,  sizeof(tvs));
    int timerfd= -1;
    timerfd = create_timer_fd();
    if (timerfd < 0) {
        LOG(FATAL)<<"create timer fd failed!";
        return NULL;
    }

    this->timerfd = timerfd;

    int ret = 0;

    this->tvs_pos = 0;

    while (stop_flag == 0)
    {
        uint64_t cnt = 0;

        update_gettimeofday_cache();

        /*
        do_in_queue();
        check_timeout();
        do_dequeue();
        */

        struct pollfd pf = { fd: timerfd, events: POLLIN | POLLERR | POLLHUP };
        ret = poll(&pf, 1, -1);
        ret = syscall(SYS_read, timerfd, &cnt, sizeof(cnt)); 
        if (ret != sizeof(cnt))
        {
            LOG(ERROR)<<"read timerfd failed."
                "[ret:"<<ret<<"]"
                "[timerfd:"<<timerfd<<"]"
                "[errno:"<<errno<<"]"
                "[errmsg:"<<strerror(errno)<<"]"
                ;
            continue;
        }
    }

    close(timerfd);
    return NULL;
}

void time_mgr_t::update_gettimeofday_cache()
{
    // 更新 gettimeofday cache
    _(gettimeofday)(tvs+tvs_pos, NULL);
    this->gettimeofday_cache = tvs+tvs_pos;
    this->tvs_pos = (tvs_pos+1)%100;
    this->cur_ms = gettimeofday_cache->tv_sec * 1000 + gettimeofday_cache->tv_usec/1000;
}

void time_mgr_t::do_in_queue()
{
    if (in_queue_num <= 0)
    {
        return ;
    }
    {
        SCOPE_LOCK(this_mutex);
        list_splice_tail_init(&timeout_notify_inqueue, &timeout_notify_queue);
        in_queue_num = 0;
    }
}

void time_mgr_t::check_timeout()
{
    timeout_notify_t *t=NULL, *next=NULL;
    uint64_t val = 1;

    int ret = 0;
    list_for_each_entry_safe(t, next, &timeout_notify_queue, link_to)
    {
        if (t->status == 3) 
        {
            list_move(&t->link_to, &timeout_notify_dequeue);
            continue;
        }

        if (t->status == 1) t->status = 2; 

        //if (t->latest_ms <= this->cur_ms && t->event_fd >= 0)
        // 都唤醒
        {
            ret = write(t->event_fd, &val, sizeof(val));
            if (ret != 8)
            {
                LOG(ERROR)<<" write event [fd:"<<t->event_fd<<"] failed!"
                    "[ret:"<<ret<<"]"
                    "[errno:"<<errno<<"]"
                    "[errmsg:"<<strerror(errno)<<"]"
                    ;
                list_move(&t->link_to, &timeout_notify_dequeue);
                continue;
            }
        }
    }
}

void time_mgr_t::do_dequeue()
{
    timeout_notify_t *t=NULL, *next=NULL;
    list_for_each_entry_safe(t, next, &timeout_notify_dequeue, link_to)
    {
        SCOPE_LOCK(this_mutex);
        list_del_init(&t->link_to);
        t->status = 4; 
        notify_pool.release(t);
    }
}

int time_mgr_t::start()
{
    this->time_resolution = FLAGS_time_resolution;

    SCOPE_LOCK(this_mutex);
    if (tid) {
        LOG(ERROR)<<"gettimeofday thread has started!";
        return -1;
    }
    tid = new pthread_t();
    void *(*proc)(void *) = NULL;
    proc = conet::ptr_cast<typeof(proc)>(&time_mgr_t::main_proc);
    pthread_create(tid, NULL, proc, this);
    return 0;
}

int time_mgr_t::stop()
{
    SCOPE_LOCK(this_mutex);
    if (NULL == tid) {
        LOG(ERROR)<<"gettimeofday thread not started!";
        return -1;
    }

    stop_flag = 1;
    pthread_join(*tid, NULL);
    delete tid;
    tid = NULL;
    return 0;
}

uint64_t get_sys_ms()
{
    if (g_time_mgr.gettimeofday_cache) {
        return g_time_mgr.cur_ms;
    } else {
        struct timeval te;
        gettimeofday(&te, NULL);
        uint64_t ms = te.tv_sec*1000UL + te.tv_usec/1000;
        return ms;
    }
}


uint64_t get_tick_ms()
{
    if (g_time_mgr.gettimeofday_cache) {
        return g_time_mgr.cur_ms;
    } else {
        struct timeval te;
        gettimeofday(&te, NULL);
        uint64_t ms = te.tv_sec*1000UL + te.tv_usec/1000;
        return ms;
    }
}


}

