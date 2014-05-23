#ifndef __TIME_HELPER_H_INC__
#define __TIME_HELPER_H_INC__

static inline
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

static inline
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

static inline
uint64_t get_tick_ms()
{
    static uint64_t khz = get_cpu_khz();
    return rdtscp() / khz;
}

static inline
uint64_t get_sys_ms()
{
    struct timeval te;
    gettimeofday(&te, NULL);
    uint64_t ms = te.tv_sec*1000UL + te.tv_usec/1000;
    return ms;
}

#define time_after(a,b) (((int64_t)(b) - (int64_t)(a))<0)
#define time_before(a,b) time_after(b, a)

#define time_after_eq(a,b) (((int64_t)(a) - (int64_t)(b))>=0)
#define time_before_eq(a,b) time_after_eq(b, a)

#define time_diff(a, b) ((int64_t)(a) - (int64_t)(b))

#endif
