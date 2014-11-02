/*
 * =====================================================================================
 *
 *       Filename:  server_main.cpp
 *
 *    Description:  
 *
 *        Version:  1.0
 *        Created:  2014年09月15日 23时20分47秒
 *       Revision:  none
 *       Compiler:  gcc
 *
 *         Author:  piboye
 *   Organization:  
 *
 * =====================================================================================
 */

#include <stdlib.h>
#include <stdio.h>
#include <errno.h>
#include <string.h>
#include <unistd.h>
#include <signal.h>
#include <malloc.h>
#include <sys/wait.h>
#include <sys/types.h>
#include <map>
#include <vector>

#include "rpc_pb_server.h"
#include "rpc_pb_server_base.h"

#include "thirdparty/glog/logging.h"
#include "thirdparty/gflags/gflags.h"
#include "base/incl/ip_list.h"
#include "base/incl/delay_init.h"
#include "base/incl/net_tool.h"
#include "base/incl/cpu_affinity.h"
#include "base/incl/auto_var.h"
#include "base/incl/gcc_builtin_help.h"
#include "base/incl/fn_ptr_cast.h"
#include <linux/netdevice.h>

//DEFINE_string(http_server_address, "", "default use server address");

DEFINE_string(server_address, "0.0.0.0:12314", "default server address");

DEFINE_int32(server_stop_wait_seconds, 2, "server stop wait seconds");
DEFINE_int32(work_num, 1, "server work num");

DEFINE_string(cpu_set, "", "cpu affinity set");

DEFINE_bool(thread_mode, false, "multithread");

namespace conet
{

typedef void server_fini_func_t(void);
std::vector<server_fini_func_t *> g_server_fini_funcs;

int registry_server_fini_func(server_fini_func_t *func)
{
    g_server_fini_funcs.push_back(func); 
    return 1;
}

std::string g_rpc_server_name;

}

#define REG_SERVER_FININSH(func) \
    static int CONET_MACRO_CONCAT(g_registry_fini_, __LINE__) = conet::registry_server_fini_func(func);


using namespace conet;

static int g_exit_flag = 0;

static
void sig_exit(int sig)
{
   g_exit_flag=1; 
}

struct TaskEnv;

struct Task
{

        conet::tcp_server_t tcp_server;    
        conet::http_server_t http_server;    
        conet::rpc_pb_server_t rpc_server;    

        ip_port_t rpc_ip_port;

        int rpc_listen_fd;

        TaskEnv *env;

        Task()
        {
            rpc_listen_fd = -1;
        }


        int init(TaskEnv *env);

        int start()
        {
            tcp_server.start();
            if (tcp_server.state == tcp_server_t::SERVER_STOPED) {
                LOG(ERROR)<<"listen to ["<<rpc_ip_port.ip.c_str()<<":"<<rpc_ip_port.port<<"], failed"; 
                return 0;
            }
            fprintf(stderr, "listen to [%s:%d]\n, success\n", rpc_ip_port.ip.c_str(), rpc_ip_port.port);
            LOG(INFO)<<"listen to ["<<rpc_ip_port.ip.c_str()<<":"<<rpc_ip_port.port<<"], success"; 
            return 0;
        }

        int stop(int timeout)
        {
            int ret = 0;
            ret = rpc_server.stop(timeout);
            return ret;
        }

};

struct TaskEnv
{
  conet::rpc_pb_server_base_t base_server;    

  pthread_t tid;
  pid_t  pid;

  int exit_finsished;
  int cpu_id;

  TaskEnv()
  {
      exit_finsished = 0;
      cpu_id = -1;
      base_server.get_global_server_cmd();
  }

  std::vector<Task *> tasks;

  void add(Task *task)
  {
      tasks.push_back(task);
  }

  void * run()
  {
      if (this->cpu_id >=0) {
          if (FLAGS_thread_mode) {
              set_cur_thread_cpu_affinity(this->cpu_id);
          } else {
              set_proccess_cpu_affinity(this->cpu_id);
          }
      }

      for(size_t i=0; i< tasks.size(); ++i)
      {
          tasks[i]->start();
      }

      coroutine_t *exit_co = NULL;

      while (likely(!this->exit_finsished)) 
      {
          if (unlikely(g_exit_flag && exit_co == NULL)) 
          {
              exit_co = conet::alloc_coroutine(conet::fn_ptr_cast<conet::co_main_func_t>(&TaskEnv::proc_server_exit), this);
              conet::resume(exit_co);
          }
          conet::dispatch();
      }

      if (exit_co) 
      {
          free_coroutine(exit_co);
      }

      return NULL;
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

  ~TaskEnv()
  {
      for(size_t i=0; i< tasks.size(); ++i)
      {
          delete tasks[i];
      }
      //tasks.clear();
  }

};

int Task::init(TaskEnv *env)
{
    this->env = env;
    int ret = 0;
    ret = tcp_server.init(rpc_ip_port.ip.c_str(), rpc_ip_port.port, rpc_listen_fd);
    ret = http_server.init(&tcp_server);
    ret = rpc_server.init(&env->base_server, &tcp_server, &http_server);

    return ret;
}

static int g_master_controller = 1;

typedef int cpu_id_type;
std::vector<int> g_cpu_set;
std::vector<ip_port_t> g_rpc_ip_list;


int proc_process_mode(int proc_num)
{
        static std::map<pid_t, cpu_id_type> g_childs;

        TaskEnv  *env = new TaskEnv();

        if (!g_cpu_set.empty()) {
            env->cpu_id = g_cpu_set[0];
        }

        for (size_t i = 0; i< g_rpc_ip_list.size(); ++i)
        {
            int rpc_listen_fd = conet::create_tcp_socket(g_rpc_ip_list[i].port, g_rpc_ip_list[i].ip.c_str(), true);
            if (rpc_listen_fd<0) {
                LOG(ERROR)<<"listen to ["<<g_rpc_ip_list[i].ip<<":"<<g_rpc_ip_list[i].port<<"failed!";
                return 0;
            }

            Task  *task = new Task();
            task->rpc_ip_port = g_rpc_ip_list[i];
            task->rpc_listen_fd= rpc_listen_fd;
            task->init(env);
            env->add(task);
        }

        int num = proc_num;

        for (int i=0; i< num; ++i)
        {
            if (!g_cpu_set.empty()) {
                env->cpu_id = g_cpu_set[i%g_cpu_set.size()];
            }
            pid_t pid = fork();
            if (pid == 0) {
                g_master_controller = 0;
                env->run();
                break;
            } else if (pid > 0) {
                g_childs[pid] = env->cpu_id;
            } else {
                LOG(ERROR)<<"for child failed";
            }
        }

        if (g_master_controller) {
            while(!g_exit_flag && g_master_controller)
            {
                int status = 0;
                pid_t pid = wait(&status);
                if (g_childs.find(pid) == g_childs.end()) {
                    continue;
                }

                LOG(ERROR)<<"work process:"<<pid<<" error exit, status:"<<status;

                sleep(1);

                LOG(INFO)<<"restore work process:";

                int cpu_id = g_childs[pid];
                g_childs.erase(pid);
                if (!g_exit_flag) {
                   pid = fork(); 
                   if (pid == 0) {
                        g_master_controller = 0;
                        env->run();
                        break;
                   } else if (pid > 0) {
                       g_childs[pid] = cpu_id;
                   } else {
                       LOG(ERROR)<<"for child failed";
                   }
                }
            }

            if (g_exit_flag && g_master_controller) {
                AUTO_VAR(it, = , g_childs.begin());
                for (; it != g_childs.end(); ++it) {
                    if (it->second > 0) { 
                        kill(it->second, SIGINT);
                    }
                }
                for (it = g_childs.begin(); it != g_childs.end(); ++it)
                {
                    int status =0;
                    if (it->second > 0) { 
                        waitpid(it->second, &status, 0);
                    }
                }
            }
        }
        delete env;

        return 0;
}


int get_listen_fd(char const *ip, int port, int listen_fd)
{
#if HAVE_SO_REUSEPORT
    
    int rpc_listen_fd = conet::create_tcp_socket(port, ip, true);
    return rpc_listen_fd;
#else
    return dup(listen_fd);
#endif
}

int proc_thread_mode(int num)
{
        TaskEnv *envs = new TaskEnv[num];
        std::vector<int> rpc_listen_fds;

        for (size_t i = 0; i< g_rpc_ip_list.size(); ++i)
        {
            int rpc_listen_fd = conet::create_tcp_socket(g_rpc_ip_list[i].port, g_rpc_ip_list[i].ip.c_str(), true);
            if (rpc_listen_fd<0) {
                LOG(ERROR)<<"listen to ["<<g_rpc_ip_list[i].ip<<":"<<g_rpc_ip_list[i].port<<"failed!";
                return 0;
            }
            rpc_listen_fds.push_back(rpc_listen_fd);
        }

        for (int i=0; i< num; ++i)
        {
            TaskEnv * env = envs+i;
            if (!g_cpu_set.empty()) {
                env->cpu_id = g_cpu_set[i%g_cpu_set.size()];
            }

            for (size_t i=0; i<g_rpc_ip_list.size(); ++i) {
                Task  *task = new Task();
                task->rpc_ip_port = g_rpc_ip_list[i];
                task->rpc_listen_fd= get_listen_fd(g_rpc_ip_list[i].ip.c_str(), g_rpc_ip_list[i].port, rpc_listen_fds[i]);
                task->init(env);
                env->add(task);
            }

            pthread_create(&env->tid, NULL, conet::fn_ptr_cast<void *(*)(void*)>(&TaskEnv::run), env);
        }

        for (int i=0; i< num; ++i)
        {
            pthread_join(envs[i].tid, NULL);
        }

        delete[] envs;
        return 0;
}

int main(int argc, char * argv[])
{
    int ret = 0;
    signal(SIGINT, sig_exit);

    mallopt(M_MMAP_THRESHOLD, 1024*1024); // 1MB，防止频繁mmap 
    mallopt(M_TRIM_THRESHOLD, 8*1024*1024); // 8MB，防止频繁brk 

    ret = google::ParseCommandLineFlags(&argc, &argv, false); 
    google::InitGoogleLogging(argv[0]);

    parse_affinity(FLAGS_cpu_set.c_str(), &g_cpu_set);

    {
        // delay init
        delay_init::call_all_level();
        LOG(INFO)<<"delay init total:"<<delay_init::total_cnt
                <<" success:"<<delay_init::success_cnt
                <<", failed:"<<delay_init::failed_cnt;

        if(delay_init::failed_cnt>0)
        {
            LOG(ERROR)<<"delay init failed, failed num:"<<delay_init::failed_cnt;
            return 1;
        }
    }

    parse_ip_list(FLAGS_server_address, &g_rpc_ip_list);
    if (g_rpc_ip_list.empty()) {
        LOG(ERROR)<<"server_addr:"<<FLAGS_server_address<<", format error!";
        return 1;
    }


    if (FLAGS_thread_mode == false && FLAGS_work_num > 0) {
        proc_process_mode(FLAGS_work_num);
    } else {
        int num = FLAGS_work_num;
        proc_thread_mode(num);
    }
    
    for(size_t i=0, len = g_server_fini_funcs.size(); i<len; ++i) 
    {
        conet::server_fini_func_t *func = conet::g_server_fini_funcs[i];
        if (func) {
            func();
        }
    }

    return 0;
}

