/*
 * =====================================================================================
 *
 *       Filename:  gethostbyname_test.cpp
 *
 *    Description:  
 *
 *        Version:  1.0
 *        Created:  2014年12月15日 03时44分59秒
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
#include "conet_all.h"
#include "gflags/gflags.h"
#include "base/plog.h"
#include "base/module.h"

#include <netdb.h>

using namespace conet;

DEFINE_int32(task_num, 10, "query time");
DEFINE_int32(num, 100, "query time");
DEFINE_string(name, "www.baidu.com", "resolve host name");

coroutine_t  *g_co = NULL;

int g_finish_cnt = 0;
int t(void *arg)
{
    for (int i=0; i< FLAGS_num; ++i) 
    {
        hostent * host = gethostbyname(FLAGS_name.c_str());
        if (host) {
            PLOG_INFO("host:", host->h_name);
        }
        else {
            PLOG_ERROR("parse ", FLAGS_name, " failed!");
        }
    }
    ++g_finish_cnt;
    return 0;
}



int main(int argc, char * argv[])
{
  InitAllModule(argc, argv);
  conet::init_conet_global_env();
  conet::init_conet_env();

  for (int i= 0; i< (int) FLAGS_task_num; ++i)
  {
      conet::coroutine_t *co = conet::alloc_coroutine(&t,  NULL);
      conet::set_auto_delete(co);
      resume(co, NULL);
  }

  while (g_finish_cnt < FLAGS_task_num) {
      conet::dispatch();
  }
  return 0;
}

