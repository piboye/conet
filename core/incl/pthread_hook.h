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

#include "coroutine_impl.h"
namespace conet
{

int is_enable_pthread_hook();
void enable_pthread_hook();
void disable_pthread_hook();

inline
int is_enable_pthread_hook()
{
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

}

#endif /* end of include guard */
