/*
 * =====================================================================================
 *
 *       Filename:  malloc_hook.cpp
 *
 *    Description:
 *
 *        Version:  1.0
 *        Created:  2014年11月20日 11时41分05秒
 *       Revision:  none
 *       Compiler:  gcc
 *
 *         Author:  piboye
 *   Organization:
 *
 * =====================================================================================
 */
#include <stdlib.h>
#include <malloc.h>
#include <sys/syscall.h>
#include <dlfcn.h>
#include "base/auto_var.h"



#define SYS_FUNC(name) g_sys_##name##_func
#define _(name) SYS_FUNC(name)

#define HOOK_FUNC_DEF(ret_type, name, proto) \
    typedef ret_type (* name##_pfn_t) proto; \
    name##_pfn_t _(name) = (name##_pfn_t) dlsym(RTLD_NEXT, #name) ; \
    ret_type name proto \

#define HOOK_FUNC(name) \
    do { \
        if( !_(name)) {  \
            _(name) = (name##_pfn_t) dlsym(RTLD_NEXT,#name);  \
        } \
    } while(0) \

namespace 
{
int64_t __thread g_in_malloc = 0;

class SetInMallocHelp
{
public:
    SetInMallocHelp()
    {
        //__sync_fetch_and_add(&g_in_malloc, 1);
        ++g_in_malloc;
    }

    ~SetInMallocHelp()
    {
        --g_in_malloc;
        //__sync_fetch_and_add(&g_in_malloc,-1);
    }
};

}

namespace conet
{

    int64_t is_in_malloc()
    {
        return g_in_malloc;
    }
}




#define DISABLE_CO_HOOK() SetInMallocHelp __set_in_malloc_##__LINE__


HOOK_FUNC_DEF(void*, malloc, (size_t size)) 
{
    HOOK_FUNC(malloc);

    DISABLE_CO_HOOK();
    return _(malloc)(size);
}

HOOK_FUNC_DEF(void *, realloc, (void *ptr, size_t size))
{
    HOOK_FUNC(realloc);
    DISABLE_CO_HOOK();
    return _(realloc)(ptr, size);
}

HOOK_FUNC_DEF(void *, memalign, (size_t  b, size_t size))
{
    HOOK_FUNC(memalign);
    DISABLE_CO_HOOK();
    return _(memalign)(b, size);
}

HOOK_FUNC_DEF(void , free, (void *ptr))
{
    HOOK_FUNC(free);
    DISABLE_CO_HOOK();
    return _(free)(ptr);
}


