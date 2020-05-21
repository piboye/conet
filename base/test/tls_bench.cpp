/*
 * =====================================================================================
 *
 *       Filename:  tls_bench.cpp
 *
 *    Description:  
 *
 *        Version:  1.0
 *        Created:  2014年10月31日 22时37分41秒
 *       Revision:  none
 *       Compiler:  gcc
 *
 *         Author:  YOUR NAME (), 
 *   Organization:  
 *
 * =====================================================================================
 */
#include <stdlib.h>
#include "tls.h"

#include "gflags/gflags.h"
#include "module.h"

DEFINE_int32(num, 100000000, "swap num");

__thread int * g_i = NULL;

CONET_DEF_TLS_VAR_HELP_DEF(g_i);

int main(int argc, char * argv[])
{
    
  InitAllModule(argc, argv);
  for(uint64_t i = (uint64_t)FLAGS_num; i > 0; --i)
  {
      ++*TLS_GET(g_i); 
  }
  return 0;
}
