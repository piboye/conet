#ifndef __TIME_HELPER_H_INC__
#define __TIME_HELPER_H_INC__
#include <stdint.h>

namespace conet
{
uint64_t rdtscp(void);

uint64_t get_cpu_khz();

uint64_t get_tick_ms();

uint64_t get_sys_ms();

// ms base conet heartbeart
uint64_t get_cached_ms();
}

using namespace conet;

#define time_after(a,b) (((int64_t)(b) - (int64_t)(a))<0)
#define time_before(a,b) time_after(b, a)

#define time_after_eq(a,b) (((int64_t)(a) - (int64_t)(b))>=0)
#define time_before_eq(a,b) time_after_eq(b, a)

#define time_diff(a, b) ((int64_t)(a) - (int64_t)(b))

#endif
