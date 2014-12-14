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


#include <vector>

namespace conet
{

class ServerTask;

class ServerWorker
{
public:
  pthread_t tid;
  pid_t  pid;

  int m_exit_flag;
  int exit_finsished;
  int cpu_id;
  int m_stop_wait_second;

  std::vector<ServerTask *> tasks;

  ServerWorker();

  int start();

  int run();

  int proc()
  {
      int ret = 0;
      ret = start();
      ret =  run();
      return ret;
  }

  int stop(int stop_wait_seconds);

  int proc_server_exit();

  ~ServerWorker();

};

}

#endif /* end of include guard */
