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

namespace conet 
{

struct coroutine_env_t
{
    list_head run_queue;
    coroutine_t * curr_co;
    coroutine_t * main;
    void * epoll_ctx;
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
    int state;
    char is_main;
    char is_enable_sys_hook;
    char is_end_delete;
    int ret_val;
    void * stack;
    CO_MAIN_FUN *pfn;
    void *pfn_arg;
    list_head wait_to;
    void *yield_val;
    char const *desc;
    gc_mgr_t * gc_mgr; // gc mem alloc manager
};

}

#endif /* end of include guard */
