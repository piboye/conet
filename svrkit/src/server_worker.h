/*
 * =====================================================================================
 *
 *       Filename:  server_worker.h
 *
 *    Description:  
 *
 *        Version:  1.0
 *        Created:  2014年12月11日 15时55分38秒
 *       Revision:  none
 *       Compiler:  gcc
 *
 *         Author:  piboye
 *   Organization:  
 *
 * =====================================================================================
 */
#ifndef __CONET_SERVER_WORKER_H__
#define __CONET_SERVER_WORKER_H__


#include <stdlib.h>

#include "base/incl/cpu_affinity.h"
#include "core/incl/conet_all.h"
#include "base/incl/ptr_cast.h"
#include "server_task.h"

namespace conet
{


class ServerWorker
{
public:
  pthread_t tid;
  pid_t  pid;

  int m_exit_flag;
  int exit_finsished;
  int cpu_id;
  std::vector<ServerTask *> tasks;

  int m_thread_mode;

  ServerWorker()
  {
      m_exit_flag = 0;
      exit_finsished = 0;
      cpu_id = -1;
  }


  void set_thread_mode()
  {
      m_thread_mode = 1;
  }


  void add(ServerTask *task)
  {
      tasks.push_back(task);
  }

  int start()
  {
      if (this->cpu_id >=0) 
      {
          if (m_thread_mode) {
              set_cur_thread_cpu_affinity(this->cpu_id);
          } else {
              set_proccess_cpu_affinity(this->cpu_id);
          }
      }

      for(size_t i=0; i< tasks.size(); ++i)
      {
          tasks[i]->start();
      }
      return 0;
  }

  int run()
  {

      coroutine_t *exit_co = NULL;

      while (likely(!this->exit_finsished)) 
      {
          if (unlikely(m_exit_flag && exit_co == NULL)) 
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

  int stop()
  {
     m_exit_flag = 1;
     return 0;
  }

  int proc_server_exit()
  {
      int wait_ms = 10000; // 10s
      for(size_t i=0; i< tasks.size(); ++i)
      {
          tasks[i]->stop(wait_ms);
      }

      this->exit_finsished = 1;
      return 0;
  }

  ~ServerWorker()
  {
      for(size_t i=0; i< tasks.size(); ++i)
      {
          delete tasks[i];
      }
      tasks.clear();
  }

};

}

#endif /* end of include guard */
