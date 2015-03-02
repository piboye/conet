/*
 * =====================================================================================
 *
 *       Filename:  time_helper_test.cpp
 *
 *    Description:  
 *
 *        Version:  1.0
 *        Created:  2014年11月01日 18时07分52秒
 *       Revision:  none
 *       Compiler:  gcc
 *
 *         Author:  YOUR NAME (), 
 *   Organization:  
 *
 * =====================================================================================
 */
#include <stdlib.h>

#include <stdlib.h>
#include "tls.h"

#include "gflags/gflags.h"
#include "time_helper.h"

DEFINE_int32(num, 1000000, "num");
DEFINE_bool(sys_ms, false, "use gettimeofday to get ms");


int main(int argc, char * argv[])
{
    
  gflags::ParseCommandLineFlags(&argc, &argv, false); 
  if (FLAGS_sys_ms)
  {
      for(uint64_t i = (uint64_t)FLAGS_num; i > 0; --i)
      {
          conet::get_sys_ms();
      }
  } else {
      for(uint64_t i = (uint64_t)FLAGS_num; i > 0; --i)
      {
          conet::get_tick_ms();
      }

  }
  return 0;
}
