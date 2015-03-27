/*
 * =====================================================================================
 *
 *       Filename:  network_hook.h
 *
 *    Description:
 *
 *        Version:  1.0
 *        Created:  2014年05月21日 17时09分00秒
 *       Revision:  none
 *       Compiler:  gcc
 *
 *         Author:  piboye
 *   Organization:
 *
 * =====================================================================================
 */
#ifndef __NETWORK_HOOK_H__
#define __NETWORK_HOOK_H__

#include "coroutine_impl.h"
#include <sys/socket.h>

namespace conet
{

void disable_sys_hook();
void enable_sys_hook();
int is_enable_sys_hook();

inline 
int is_enable_sys_hook()
{
    coroutine_t *co = get_curr_co_can_null();
    return co && !co->is_main && (co->is_enable_sys_hook);
}

inline
void enable_sys_hook()
{

    coroutine_t *co = get_curr_co_can_null();
    if (co) 
        co->is_enable_sys_hook = 1;
}

inline
void disable_sys_hook()
{
    coroutine_t *co = get_curr_co_can_null();
    if (co) 
        co->is_enable_sys_hook = 0;
}


int  my_accept4( int fd, struct sockaddr *addr, socklen_t *len, int flags);

}

#endif /* end of include guard */
