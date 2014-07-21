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

namespace conet
{
uint64_t rdtscp(void)
{
    register uint32_t lo, hi;
    register uint64_t o;
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

uint64_t get_sys_ms()
{
    struct timeval te;
    gettimeofday(&te, NULL);
    uint64_t ms = te.tv_sec*1000UL + te.tv_usec/1000;
    return ms;
}

uint64_t __thread g_cached_ms = 0; 
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

}

