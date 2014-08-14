/*
 * =====================================================================================
 *
 *       Filename:  time_helper.cpp
 *
 *    Description:  
 *
 *        Version:  1.0
 *        Created:  2014年07月18日 14时34分47秒
 *       Revision:  none
 *       Compiler:  gcc
 *
 *         Author:  YOUR NAME (), 
 *   Organization:  
 *
 * =====================================================================================
 */
#include "time_helper.h"
#include "dispatch.h"

#include <stdlib.h>
#include <stdio.h>
#include <string.h>
#include <sys/time.h>
#include "hook_helper.h"
#include "log.h"
#include "conet_all.h"
#include <sys/timerfd.h>  

namespace conet
{
inline
uint64_t rdtscp(void)
{
    uint64_t lo, hi;
    uint64_t o;
    __asm__ __volatile__ (
        "rdtscp" : "=a"(lo), "=d"(hi)
    );
    o = hi;
    o <<= 32;
    return (o | lo);
}

uint64_t get_cpu_khz()
{
    FILE *fp = fopen("/proc/cpuinfo","r");
    if(!fp) return 1;
    char buf[4096] = {0};
    fread(buf,1,sizeof(buf),fp);
    fclose(fp);

    char *lp = strstr(buf,"cpu MHz");
    if(!lp) return 1;
    lp += strlen("cpu MHz");
    while(*lp == ' ' || *lp == '\t' || *lp == ':')
    {
        ++lp;
    }

    double mhz = atof(lp);
    uint64_t u = (uint64_t)(mhz * 1000);
    return u;
}


uint64_t get_tick_ms()
{
    static uint64_t khz = get_cpu_khz();
    return rdtscp() / khz;
}

HOOK_DECLARE(int ,gettimeofday,(struct timeval *tv, struct timezone *tz));

static struct timeval g_pre_te;

uint64_t g_cached_ms = 0;

uint64_t get_sys_ms()
{
    struct timeval te;
    gettimeofday(&te, NULL);
    uint64_t ms = te.tv_sec*1000UL + te.tv_usec/1000;
    return ms;
}

uint64_t get_cached_ms()
{
    if (g_cached_ms == 0) {
        g_cached_ms = get_sys_ms();
    }
    return g_cached_ms;
}

void update_cached_ms()
{
    g_cached_ms = get_sys_ms();
}


static uint64_t g_prev_tk = 0;
static uint64_t g_khz = 0;

#define LOG_SYS_CALL(func, ret) \
        LOG(ERROR)<<"syscall "<<#func <<" failed, [ret:"<<ret<<"]" \
                    "[errno:"<<errno<<"]" \
                    "[errmsg:"<<strerror(errno)<<"]" \
                    ; \

static 
int update_ms_task(void *arg)
{
    conet::enable_sys_hook(); 
    int timerfd = 0;
    timerfd = timerfd_create(CLOCK_MONOTONIC, TFD_NONBLOCK);
    if (timerfd < 0) {
        LOG_SYS_CALL(timerfd_create, timerfd); 
        return -1;
    }
    int ret = 0; 
    struct timespec now;  
    ret =  clock_gettime(CLOCK_REALTIME, &now);
    if (ret < 0) {
        LOG_SYS_CALL(clock_gettime, ret); 
        return -2;
    }

    struct itimerspec ts;
    ts.it_value.tv_sec = now.tv_sec; 
    ts.it_value.tv_nsec = ((now.tv_nsec/1000)+1)*1000;
    ts.it_interval.tv_sec = 0;
    ts.it_interval.tv_nsec = 1000;

    ret = timerfd_settime(timerfd, 0, &ts, NULL);
    if (ret < 0) {
        LOG_SYS_CALL(timerfd_settime, ret);
        return -3;
    }
    
    uint64_t cnt = 0;
    while (1) {
       struct pollfd pf = {
            fd: timerfd,
            events: POLLIN | POLLERR | POLLHUP
       };
       ret = poll(&pf, 1, -1);
       if (ret == 0) {
            break;
       }
       if (pf.revents & POLLERR) {
           break;
       }
       ret = read(timerfd, &cnt, sizeof(cnt)); 
       if (ret != sizeof(cnt)) {
           LOG(ERROR)<<"getimeofdate update task read timerfd failed";
           continue;
       }
       ret = _(gettimeofday)(&g_pre_te, NULL);
       conet::update_cached_ms();
    }
    return 0;
}

static coroutine_t * g_update_ms_task=NULL;

HOOK_SYS_FUNC_DEF(int ,gettimeofday,(struct timeval *tv, struct timezone *tz))
{
    HOOK_SYS_FUNC(gettimeofday);
    if (tz != NULL) return _(gettimeofday)(tv, tz);

    int ret = 0;

    if (g_khz == 0) {
         g_khz = get_cpu_khz();
         g_prev_tk = rdtscp();
         ret = _(gettimeofday)(&g_pre_te, NULL);
         g_update_ms_task = alloc_coroutine(update_ms_task, NULL);
    }

    /*
    uint64_t cur_tk = rdtscp();

    if ((cur_tk - g_prev_tk) >= (g_khz)) {
        g_prev_tk = cur_tk; 
        ret = _(gettimeofday)(&g_pre_te, NULL);
    }
    memcpy(tv, &g_pre_te, sizeof(*tv));
    */

    tv->tv_sec = g_pre_te.tv_sec;
    tv->tv_usec = g_pre_te.tv_usec;
    return ret;
}

}

