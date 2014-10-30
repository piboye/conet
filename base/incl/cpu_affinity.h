/*
 * =====================================================================================
 *
 *       Filename:  cpu_affinity.h
 *
 *    Description:  
 *
 *        Version:  1.0
 *        Created:  2014年10月29日 05时33分48秒
 *       Revision:  none
 *       Compiler:  gcc
 *
 *         Author:  piboye
 *   Organization:  
 *
 * =====================================================================================
 */
#ifndef __CONET_CPU_AFFINITY_H__
#define __CONET_CPU_AFFINITY_H__
#include <vector>

namespace conet
{
    int parse_affinity(char const * txt, std::vector<int> *cpu_sets);
    int set_cur_thread_cpu_affinity(int cpu_id);
    int set_proccess_cpu_affinity(int cpu_id);
}

#endif /* end of include guard */
