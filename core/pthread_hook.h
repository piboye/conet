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


struct pthread_mgr_t;

void free_pthread_mgr(pthread_mgr_t*);
}

#endif /* end of include guard */
