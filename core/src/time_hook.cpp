/*
 * =====================================================================================
 *
 *       Filename:  time_hook.cpp
 *
 *    Description:
 *
 *        Version:  1.0
 *        Created:  03/13/2015 09:09:18 PM
 *       Revision:  none
 *       Compiler:  gcc
 *
 *         Author:  piboye
 *   Organization:
 *
 * =====================================================================================
 */

#include <stdlib.h>
#include "hook_helper.h"
#include <pthread.h>
#include <string.h>
#include <sys/time.h>
#include <unistd.h>
#include "time_mgr.h"
#include "network_hook.h"

HOOK_CPP_FUNC_DEF(int , gettimeofday, (struct timeval *tv, struct timezone *tz))
{
    HOOK_SYS_FUNC(gettimeofday);
    timeval *sys_tv = conet::time_mgr_t::instance().gettimeofday_cache;
    if (tz != NULL || NULL == sys_tv)  {
        return _(gettimeofday)(tv, tz);
    }
    if (!conet::is_enable_sys_hook()) {
        return _(gettimeofday)(tv, tz);
    }

    int ret = 0;
    tv->tv_sec = sys_tv->tv_sec;
    tv->tv_usec = sys_tv->tv_usec;
    return ret;
}


HOOK_CPP_FUNC_DEF(time_t, time, (time_t *out))
{
    HOOK_SYS_FUNC(time);
    timeval *sys_tv = conet::time_mgr_t::instance().gettimeofday_cache;
    if (NULL == sys_tv)
    {
        return _(time)(out);
    }

    if (out) {
        *out =  sys_tv->tv_sec;
    }
    return sys_tv->tv_sec;
}

