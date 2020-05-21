/*
 * =====================================================================================
 *
 *       Filename:  co_swap_test.cpp
 *
 *    Description:  
 *
 *        Version:  1.0
 *        Created:  2014年10月30日 06时59分08秒
 *       Revision:  none
 *       Compiler:  gcc
 *
 *         Author:  piboye
 *   Organization:  
 *
 * =====================================================================================
 */
#include <stdlib.h>
#include "conet_all.h"
#include "gflags/gflags.h"
#include "../base/module.h"

using namespace conet;

namespace conet
{
void print_stacktrace(coroutine_t *co, int fd, int baddr);
}

coroutine_t  *g_co = NULL;

int t(void *arg)
{
    conet::print_stacktrace(g_co, 2, 1);
    uint64_t i =  0;
    do {
        i = (uint64_t)yield();
    } while (i >0);
    return 0;
}

int t2(void * arg)
{
  uint64_t num = (uint64_t)(arg);
  coroutine_t  *co = conet::alloc_coroutine(&t,  NULL);
  for(uint64_t i = (uint64_t)num; i > 0; --i)
  {
      resume(co, (void *)(i));
  }
  resume(co, NULL);
  conet::free_coroutine(co);
  return 0;
}

DEFINE_int32(num, 1000000, "swap num");

int main(int argc, char * argv[])
{
  gflags::ParseCommandLineFlags(&argc, &argv, false); 
  InitAllModule(argc, argv);
  conet::init_conet_global_env();
  conet::init_conet_env();
  g_co = conet::alloc_coroutine(&t2,  (void *)(uint64_t)FLAGS_num);
  resume(g_co, NULL);
  //conet::print_stacktrace(g_co, 2);
  conet::wait(g_co);
  conet::free_coroutine(g_co);
  return 0;
}

