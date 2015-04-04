/*
 * =====================================================================================
 *
 *       Filename:  pthread_hook.h
 *
 *    Description:
 *
 *        Version:  1.0
 *        Created:  2014年05月21日 17时07分33秒
 *       Revision:  none
 *       Compiler:  gcc
 *
 *         Author:  piboye
 *   Organization:
 *
 * =====================================================================================
 */
#ifndef __PTHREAD_HOOK_H__
#define __PTHREAD_HOOK_H__

#include "coroutine_env.h"
#include "base/auto_var.h"
#include "base/addr_map.h"
#include "event_notify.h"

namespace conet
{

int64_t is_in_malloc();

int is_enable_pthread_hook();
void enable_pthread_hook();
void disable_pthread_hook();

inline
int is_enable_pthread_hook()
{
    if (is_in_malloc()) {
        return 0;
    }
    coroutine_t *co = get_curr_co_can_null();
    return co && !co->is_main && (co->is_enable_pthread_hook);
}

inline
void enable_pthread_hook()
{
    coroutine_t *co = get_curr_co_can_null();
    if (co) 
        co->is_enable_pthread_hook = 1;
}

inline
void disable_pthread_hook()
{
    coroutine_t *co = get_curr_co_can_null();
    if (co) 
        co->is_enable_pthread_hook = 0;
}

inline void disable_pthread_hook_save(int *stat) 
{
    coroutine_t *co = get_curr_co_can_null();
    if (co) { 
        *stat = co->is_enable_pthread_hook;
        co->is_enable_pthread_hook = 0;
    } else {
        *stat = 0;
    }
}
inline void restore_pthread_hook_stat(int stat)
{
    coroutine_t *co = get_curr_co_can_null();
    if (co) { 
        co->is_enable_pthread_hook = stat;
    }
}

class DisablePthreadHook
{
public:
    int m_stat;
    DisablePthreadHook()
    {
        m_stat = 0;
        disable_pthread_hook_save(&m_stat);
    }

    ~DisablePthreadHook()
    {
        restore_pthread_hook_stat(m_stat);
    }
};

struct pcond_ctx_t;
struct lock_ctx_t;
struct pthread_join_ctx_t;
struct pthread_mgr_t
{
    list_head lock_schedule_queue;   // lock schdule queue;
    list_head pcond_schedule_queue;  // pthread condition var notify schedule queue
    pthread_mutex_t pcond_schedule_mutex;

    list_head pthread_join_queue; // pthread join queue;
    conet::task_t schedule_task;     // 调度任务
    event_notify_t event_notify;

    coroutine_env_t * co_env;

    explicit pthread_mgr_t(coroutine_env_t *);


    static int event_cb(void *arg, uint64_t num);

    void add_to_pcond_schedule(pcond_ctx_t *ctx);


    void add_lock_schedule(lock_ctx_t *ctx) ;

    void add_to_pthread_join_schedule(pthread_join_ctx_t *ctx);

    ~pthread_mgr_t();

    int proc_pthread_schedule();

};

}

#endif /* end of include guard */
