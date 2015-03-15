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
#include "gflags/gflags.h"
#include <pthread.h>
#include "base/defer.h"
#include <string.h>
#include <sys/time.h>
#include <unistd.h>
#include "glog/logging.h"

namespace conet
{
    DEFINE_bool(gettimeofday, false, "enable gettimeofday improve");
}

static
struct timeval * g_sys_tv = NULL;

static
int32_t g_stop_gettimeoday_flag = 0;

static
void * gettimeofday_main_proc(void *)
{
    int max_len = 100;
    static timeval *tvs = new struct timeval[max_len];
    memset(tvs, 0,  sizeof(timeval) * max_len);

    int pos = 0;
    while (g_stop_gettimeoday_flag == 0)
    {
        gettimeofday(tvs+pos, NULL);
        g_sys_tv = tvs+pos;
        pos = (pos+1)%100;
        usleep(1000);
    }

    delete tvs;
    return NULL;
}

static pthread_t *g_gtd_pid = NULL;
pthread_mutex_t g_gtd_mutex = PTHREAD_MUTEX_INITIALIZER; 

namespace conet
{

int start_gettimeofday_improve() 
{
    pthread_mutex_lock(&g_gtd_mutex);
    CONET_DEFER((g_gtd_mutex), {
        pthread_mutex_unlock(&g_gtd_mutex);
    });
    if (g_gtd_pid) {
        LOG(ERROR)<<"gettimeofday thread has started!";
        return -1;
    }
    g_gtd_pid = new pthread_t();
    pthread_create(g_gtd_pid, NULL, gettimeofday_main_proc, NULL);
    return 0;
}

int stop_gettimeofday_improve()
{
    pthread_mutex_lock(&g_gtd_mutex);
    CONET_DEFER((g_gtd_mutex), {
        pthread_mutex_unlock(&g_gtd_mutex);
    });
    if (NULL == g_gtd_pid) {
        LOG(ERROR)<<"gettimeofday thread not started!";
        return -1;
    }
    g_stop_gettimeoday_flag = 1;
    pthread_join(*g_gtd_pid, NULL);
    delete g_gtd_pid;
    g_gtd_pid = NULL;
    return 0;
}

}

HOOK_CPP_FUNC_DEF(int , gettimeofday, (struct timeval *tv, struct timezone *tz))
{
    HOOK_SYS_FUNC(gettimeofday);
    if (tz != NULL || NULL == g_sys_tv)  {
        return _(gettimeofday)(tv, tz);
    }

    int ret = 0;
    tv->tv_sec = g_sys_tv->tv_sec;
    tv->tv_usec = g_sys_tv->tv_usec;
    return ret;
}
