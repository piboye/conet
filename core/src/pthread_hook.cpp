/*
 * =====================================================================================
 *
 *       Filename:  hook_pthread.cpp
 *
 *    Description:
 *
 *        Version:  1.0
 *        Created:  2014年05月20日 16时06分13秒
 *       Revision:  none
 *       Compiler:  gcc
 *
 *         Author:  piboyeliu
 *   Organization:
 *
 * =====================================================================================
 */
#include <stdlib.h>
#include <pthread.h>
#include "coroutine.h"
#include "coroutine_impl.h"
#include <sys/syscall.h>
#include <dlfcn.h>

#define SYS_FUNC(name) g_sys_##name##_func
#define _(name) SYS_FUNC(name)

#define HOOK_FUNC_DEF(ret_type, name, proto) \
    typedef ret_type (* name##_pfn_t) proto; \
    name##_pfn_t _(name) = (name##_pfn_t) dlsym(RTLD_NEXT, #name) ; \
    ret_type name proto __THROW \
 

#define HOOK_FUNC(name) if( !_(name)) { _(name) = (name##_pfn_t)dlsym(RTLD_NEXT,#name); }


namespace conet
{
int is_enable_pthread_hook()
{
    return current_coroutine()->is_enable_pthread_hook;
}

void enable_pthread_hook()
{
    current_coroutine()->is_enable_pthread_hook = 1;
}

void disable_pthread_hook()
{
    current_coroutine()->is_enable_pthread_hook = 0;
}

}

HOOK_FUNC_DEF(
    void*, pthread_getspecific, (pthread_key_t key)
)
{
    HOOK_FUNC(pthread_getspecific);
    if (!conet::is_enable_pthread_hook()) {
        return _(pthread_getspecific)(key);
    }

    return conet::get_pthread_spec(key);
}

HOOK_FUNC_DEF(
    int, pthread_setspecific, (pthread_key_t key, __const void *value)
)
{
    HOOK_FUNC(pthread_setspecific);
    if (!conet::is_enable_pthread_hook()) {
        return _(pthread_setspecific)(key, value);
    }

    return conet::set_pthread_spec(key, value);
}

