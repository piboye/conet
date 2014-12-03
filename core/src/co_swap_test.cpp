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

using namespace conet;

int t(void *arg)
{
    uint64_t i =  0;
    do {
        i = (uint64_t)yield();
    } while (i >0);
    return 0;
}

DEFINE_int32(num, 1000000, "swap num");

int main(int argc, char * argv[])
{
    
  google::ParseCommandLineFlags(&argc, &argv, false); 
  coroutine_t  *co = conet::alloc_coroutine(&t,  NULL);
  int num = FLAGS_num;
  for(uint64_t i = (uint64_t)num; i > 0; --i)
  {
      resume(co, (void *)(i));
  }
  resume(co, NULL);
  return 0;
}

