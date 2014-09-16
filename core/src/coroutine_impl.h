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
 *         Author:  YOUR NAME (),
 *   Organization:
 *
 * =====================================================================================
 */
#ifndef __COROUTINE_IMPL_H_INCL__
#define __COROUTINE_IMPL_H_INCL__
#include "list.h"
#include <ucontext.h>
#include "gc.h"
#include "timewheel.h"
#include "coroutine.h"
#include <map>

namespace conet
{

struct epoll_ctx_t;
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
    void * stack;

    int state;

    struct {
        unsigned int is_main:1;
        unsigned int is_enable_sys_hook:1;
        unsigned int is_end_delete:1;
        unsigned int is_enable_pthread_hook:1;
        unsigned int is_enable_disk_io_hook:1;
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

    std::map<void *, void*> *static_vars;
    std::map<uint64_t, void *> * spec;
    std::map<pthread_key_t, void *> * pthread_spec;

};

}

#endif /* end of include guard */
