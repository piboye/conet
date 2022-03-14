/*
 * =====================================================================================
 *
 *       Filename:  coroutine_env.h
 *
 *    Description:
 *
 *        Version:  1.0
 *        Created:  2014年05月06日 07时21分13秒
 *       Revision:  none
 *       Compiler:  gcc
 *
 *         Author:  piboye
 *   Organization:
 *
 * =====================================================================================
 */
#ifndef __COROUTINE_IMPL_H_INCL__
#define __COROUTINE_IMPL_H_INCL__

#include <ucontext.h>
#include <map>
#include <sys/epoll.h>

#include "gc.h"
#include "timewheel.h"
#include "coroutine.h"
#include "gflags/gflags.h"
#include "base/fixed_mempool.h"
#include "base/tls.h"
#include "base/list.h"
#include "fcontext.h"

#ifdef USE_VALGRIND
#include "valgrind/valgrind.h"
#endif

DECLARE_int32(stack_size);

namespace conet
{

struct epoll_ctx_t
{
    int m_epoll_fd;
    int m_epoll_size;

    epoll_event *m_epoll_events;
    int m_mem_size;

    int wait_num;
    task_t task;
    coroutine_env_t * co_env;
};

struct fd_ctx_mgr_t;

struct dispatch_mgr_t;
struct timewheel_t;
struct pthread_mgr_t;

struct coroutine_env_t
{
    list_head run_queue;
    coroutine_t * curr_co;
    coroutine_t * main;
    list_head tasks;
    uint64_t spec_key_seed;
    epoll_ctx_t *epoll_ctx;
    dispatch_mgr_t *dispatch;
    timewheel_t  * tw;

    pthread_mgr_t * pthread_mgr;

    fixed_mempool_t default_stack_pool;
    

    // 延迟删除 coroutine 对象
    list_head delay_del_list;
    task_t delay_del_task;

    //corotuine_t 结构体 缓存池
    fixed_mempool_t co_struct_pool;

    coroutine_env_t();
    ~coroutine_env_t();
}
__attribute__((aligned(64)))
;


enum {
    CREATE=0,
    RUNNING=1,
    SUSPEND = 2,
    STOP=3,
    EXCEPTION_EXIT=-1,
};

struct coroutine_t
{
    fcontext_t *fctx;
    
    void * stack;
    uint64_t stack_size;

    CO_MAIN_FUN *pfn;
    void *pfn_arg;

    coroutine_env_t *env;
    char const *desc;
    gc_mgr_t * gc_mgr; // gc mem alloc manager


    list_head wait_to;

    list_head exit_notify_queue; // get notify on the co exit;

    uint64_t id;


    std::map<void *, void*> *static_vars;
    std::map<uint64_t, void *> * spec;
    std::map<pthread_key_t, void *> * pthread_spec;

    int state;
    int ret_val;
    struct {
        unsigned int is_main:1;
        unsigned int is_enable_sys_hook:1;
        unsigned int is_end_delete:1;
        unsigned int is_enable_pthread_hook:1;
        unsigned int is_enable_disk_io_hook:1;
        unsigned int is_page_stack:1;
    };
#ifdef USE_VALGRIND
    int m_vid;
#endif
}
__attribute__((aligned(64)))
;

extern __thread coroutine_env_t * g_coroutine_env;


// 协程环境 数组, 方便调式使用
extern coroutine_env_t ** g_coroutine_envs;

inline
coroutine_env_t * get_coroutine_env()
{
	return g_coroutine_env;
}

inline 
coroutine_t *get_curr_co_can_null()
{
    coroutine_env_t *env = g_coroutine_env;
    if (unlikely(NULL == env))
    {
        return NULL;
    }
    return env->curr_co;
}


}

#endif /* end of include guard */
