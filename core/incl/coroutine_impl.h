/*
 * =====================================================================================
 *
 *       Filename:  coroutine_impl.h
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
#include "gc.h"
#include "timewheel.h"
#include "coroutine.h"
#include <map>
#include <sys/epoll.h>
#include "gflags/gflags.h"
#include "../../base/incl/fixed_mempool.h"
#include "../../base/incl/tls.h"
#include "../../base/incl/list.h"
#include "fcontext.h"

#ifdef USE_VALGRIND
#include "/data/home/piboyeliu/soft/valgrind-3.10.0/include/valgrind.h"
#endif

DECLARE_int32(stack_size);

namespace conet
{

//struct epoll_ctx_t;
struct epoll_ctx_t
{
    int m_epoll_fd;
    int m_epoll_size;

    epoll_event *m_epoll_events;
    int m_mem_size;

    int wait_num;
};

struct fd_ctx_mgr_t;

struct coroutine_env_t
{
    list_head run_queue;
    coroutine_t * curr_co;
    coroutine_t * main;
    list_head tasks;
    uint64_t spec_key_seed;
};


enum {
    CREATE=0,
    RUNNING=1,
    SUSPEND = 2,
    STOP=3,
    EXCEPTION_EXIT=-1,
};

struct coroutine_t
{
    ucontext_t ctx;
    fcontext_t *fctx;
    
    void * stack;
    int stack_size;

    int state;

    struct {
        unsigned int is_main:1;
        unsigned int is_enable_sys_hook:1;
        unsigned int is_end_delete:1;
        unsigned int is_enable_pthread_hook:1;
        unsigned int is_enable_disk_io_hook:1;
        unsigned int is_page_stack:1;
    };

    int ret_val;
    void *yield_val;

    CO_MAIN_FUN *pfn;
    void *pfn_arg;

    char const *desc;
    gc_mgr_t * gc_mgr; // gc mem alloc manager


    list_head wait_to;

    list_head exit_notify_queue; // get notify on the co exit;

    uint64_t id;

#ifdef USE_VALGRIND
    int m_vid;
#endif

    std::map<void *, void*> *static_vars;
    std::map<uint64_t, void *> * spec;
    std::map<pthread_key_t, void *> * pthread_spec;

};

coroutine_env_t *alloc_coroutine_env();
void free_coroutine_env(void *arg);

extern __thread coroutine_env_t * g_coroutine_env;

inline
coroutine_env_t * get_coroutine_env()
{
    coroutine_env_t *env = g_coroutine_env;
    if (unlikely(NULL == env)) {
        env = alloc_coroutine_env();
        tls_onexit_add(env, &free_coroutine_env);
        g_coroutine_env = env;
    }
    return env;
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
