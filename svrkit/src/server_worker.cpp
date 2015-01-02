/*
 * =====================================================================================
 *
 *       Filename:  server_worker.cpp
 *
 *    Description:  
 *
 *        Version:  1.0
 *        Created:  2014年12月12日 08时35分12秒
 *       Revision:  none
 *       Compiler:  gcc
 *
 *         Author:  piboye
 *   Organization:  
 *
 * =====================================================================================
 */
#include <stdlib.h>
#include "server_worker.h"
#include "server_task.h"
#include "base/cpu_affinity.h"
#include "core/conet_all.h"
#include "base/ptr_cast.h"
#include "server_common.h"
#include "thirdparty/glog/logging.h"

namespace conet
{
  ServerWorker::ServerWorker()
  {
      m_exit_flag = 0;
      exit_finsished = 0;
      cpu_id = -1;
      m_stop_wait_second = 2;
  }

  int ServerWorker::start()
  {
      if (this->cpu_id >=0) 
      {
          if (is_thread_mode()) {
              set_cur_thread_cpu_affinity(this->cpu_id);
          } else {
              set_proccess_cpu_affinity(this->cpu_id);
          }
      }

      ServerTask::get_all_task(&this->tasks);

      if (tasks.empty()) {
           LOG(ERROR)<<"no server task";
      }

      for(size_t i=0; i< tasks.size(); ++i)
      {
          tasks[i]->start();
      }

      return 0;
  }


  int ServerWorker::run()
  {

      coroutine_t *exit_co = NULL;

      while (likely(!this->exit_finsished)) 
      {
          if (unlikely(get_server_stop_flag() && exit_co == NULL)) 
          {
              exit_co = conet::alloc_coroutine(conet::ptr_cast<conet::co_main_func_t>(&ServerWorker::proc_server_exit), this);
              conet::resume(exit_co);
          }
          conet::dispatch();
      }

      if (exit_co) 
      {
          free_coroutine(exit_co);
      }

      return 0;
  }

  int ServerWorker::stop(int stop_wait_seconds)
  {
     m_exit_flag = 1;
     m_stop_wait_second = stop_wait_seconds; 
     return 0;
  }

  int ServerWorker::proc_server_exit()
  {
      int wait_ms = 10000 * m_stop_wait_second; // 10s
      for(size_t i=0; i< tasks.size(); ++i)
      {
          tasks[i]->stop(wait_ms);
      }

      this->exit_finsished = 1;
      return 0;
  }

  ServerWorker::~ServerWorker()
  {
      for(size_t i=0; i< tasks.size(); ++i)
      {
          delete tasks[i];
      }
      tasks.clear();
  }

}
