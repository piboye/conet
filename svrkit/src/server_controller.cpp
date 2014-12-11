/*
 * =====================================================================================
 *
 *       Filename:  server_controller.cpp
 *
 *    Description:  
 *
 *        Version:  1.0
 *        Created:  2014年12月11日 17时14分29秒
 *       Revision:  none
 *       Compiler:  gcc
 *
 *         Author:  piboye
 *   Organization:  
 *
 * =====================================================================================
 */
#include "server_controller.h"
#include "server_worker.h"
#include "base/incl/cpu_affinity.h"
#include "base/incl/auto_var.h"
#include "base/incl/gcc_builtin_help.h"
#include "base/incl/ptr_cast.h"
#include "thirdparty/glog/logging.h"

#include <pthread.h>
#include <sys/wait.h>


namespace conet
{
// 进程模式
class ServerControllerProcessMode
    :public ServerController
{
public:

    coroutine_t * m_montior_co;

    ServerControllerProcessMode()
    {
        m_montior_co = NULL;
    }

    virtual int start()
    {
        int ret = 0;
        ret = ServerController::start();

        int num = m_curr_num;
        // 创建 worker 进程
        for (int i=0; i< num; ++i)
        {
            ServerWorker  *worker = new ServerWorker();
            if (!m_cpu_set.empty()) {
                worker->cpu_id = m_cpu_set[i%m_cpu_set.size()];
            }
            m_workers.push_back(worker);
            pid_t pid = fork();
            if (pid == 0) {
                // child
                m_worker_mode = 1;

                worker->start();
                worker->run();
                return 0;
            } else if (pid > 0) {
                worker->pid = pid;
                m_worker_map[pid] = worker;
            } else {
                LOG(ERROR)<<"for child failed";
            }
        }

        if (!m_worker_mode) 
        {
            m_montior_co = conet::alloc_coroutine(
                    conet::ptr_cast<conet::co_main_func_t>(
                        &ServerControllerProcessMode::wait_childs), this);

            conet::resume(m_montior_co);
        }

        return 0;
    }

    virtual int stop()
    {
        int ret = 0; 
        ret = ServerController::stop();
        kill_worker();
        return 0;
    }

    int wait_childs()
    {
        while(m_stop_flag)  
        {
            usleep(10000);
            int status = 0;
            pid_t pid = waitpid(-1, &status, WNOHANG);

            if (pid < 0) continue;

            if (m_worker_map.find(pid) == m_worker_map.end()) {
                continue;
            }

            if (status != 0)
            {
                LOG(ERROR)<<"work process:"<<pid<<" error exit, status:"<<status;
            }
            else {
                LOG(INFO)<<"work process:"<<pid<<" succes exit, status:"<<status;
            }

            if (m_stop_flag) {
                return 0;
            }


            LOG(INFO)<<"restore work process:";

            ServerWorker *worker = m_worker_map[pid];
            m_worker_map.erase(pid);

            if (!m_stop_flag) {
                pid = fork(); 
                if (pid == 0) {
                    m_worker_mode = 1;
                    worker->start();
                    worker->run();
                    return 0;;
                } else if (pid > 0) {
                    m_worker_map[pid] = worker;
                } else {
                    LOG(ERROR)<<"for child failed";
                }
            }
        }

        return 0;
    }

    int kill_worker()
    {
        AUTO_VAR(it, = , m_workers.begin());
        for (size_t i=0; i< m_workers.size(); ++i)
        {
            if (m_workers[i]->pid > 0) 
            { 
                kill(m_workers[i]->pid, SIGINT);
            }
        }

        for (size_t i=0; i< m_workers.size(); ++i)
        {
            int status =0;
            if (m_workers[i]->pid > 0) 
            {
                waitpid(m_workers[i]->pid, &status, 0);
            }
        }
        return 0;
    }
};


class ServerControllerThreadMode
:public ServerController
{
public:

    int start()
    {

        int ret = 0;
        ret = ServerController::start();
        int num = m_curr_num;

        for (int i=0; i< num; ++i)
        {
            ServerWorker  *worker = new ServerWorker();
            worker->set_thread_mode();
            m_workers.push_back(worker);
            if (!m_cpu_set.empty()) {
                worker->cpu_id = m_cpu_set[i%m_cpu_set.size()];
            }

            pthread_create(&worker->tid, NULL, 
                    conet::ptr_cast<void *(*)(void*)>(&ServerWorker::run), worker);
        }

        for (int i=0; i<num; ++i)
        {
            delete m_workers[i];
        }
        return 0;
    }

    int stop()
    {
        int ret = 0;
        ret = ServerController::stop();
        for (size_t i=0; i< m_workers.size(); ++i)
        {
            pthread_join(m_workers[i]->tid, NULL);
        }
        return 0;
    }
};

ServerController * ServerController::create(int thread_mode)
{
    ServerController * server = NULL;
    if (thread_mode) {
        server =  new ServerControllerThreadMode();
    } else {
        server =  new  ServerControllerThreadMode();
    }
    return server;
}

ServerController::~ServerController()
{
    for (size_t i=0; i<m_workers.size(); ++i)
    {
        delete m_workers[i];
    }
}

int ServerController::run()
{

    while (likely(!this->m_stop_flag)) 
    {
        conet::dispatch();
    }
    return 0;
}

int ServerController::stop()
{
    m_stop_flag = 1;
    for (size_t i=0; i<m_workers.size(); ++i)
    {
        m_workers[i]->m_exit_flag = 1;
    }
    return 0;
}

}
