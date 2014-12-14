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
#include "core/incl/conet_all.h"

#include "thirdparty/glog/logging.h"
#include "thirdparty/gflags/gflags.h"

#include <pthread.h>
#include <sys/wait.h>

#include "server_common.h"


namespace conet
{

DEFINE_int32(work_num, 1, "server work num");
DEFINE_string(cpu_set, "", "cpu affinity set");

int get_epoll_pend_task_num();
ServerController::ServerController()
{
    m_worker_mode = 0;
    m_stop_flag = 0;
    m_stop_finished = 0;
    m_curr_num = FLAGS_work_num;

    parse_affinity(FLAGS_cpu_set.c_str(), &m_cpu_set);
    //get_epoll_pend_task_num();
}

// 进程模式
class ServerControllerProcessMode
    :public ServerController
{
public:

    ServerWorker *m_cur_worker;

    ServerControllerProcessMode()
    {
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
                m_cur_worker = worker;
                worker->start();
                worker->run();
                return 0;
            } else if (pid > 0) {
                worker->pid = pid;
                m_worker_map[pid] = worker;
            } else {
                LOG(ERROR)<<"fork worker failed";
            }
        }

        return 0;
    }

    virtual int stop(int wait_seconds)
    {
        int ret = 0; 
        if (m_worker_mode) {
           m_cur_worker->stop(wait_seconds); 
        }else {
            ret = ServerController::stop(wait_seconds);
            kill_worker();
        }
        m_stop_finished = 1;
        return 0;
    }



    int wait_childs()
    {
        int status = 0;
        pid_t pid = waitpid(-1, &status, WNOHANG);

        if (pid < 0) return 0;

        if (m_worker_map.find(pid) == m_worker_map.end()) {
            return 0;
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
                m_cur_worker = worker;
                worker->start();
                worker->run();
                return 0;;
            } else if (pid > 0) {
                m_worker_map[pid] = worker;
            } else {
                LOG(ERROR)<<"for child failed";
            }
        }
        return 0;
    }

    virtual int run()
    {
        int ret = 0;
        if (m_worker_mode)
        {
            return 0;
        } else {
            while (!m_stop_flag) 
            {
                wait_childs();
                if (!m_stop_flag) sleep(1);
            }
        }
        return ret;
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
            m_workers.push_back(worker);
            if (!m_cpu_set.empty()) {
                worker->cpu_id = m_cpu_set[i%m_cpu_set.size()];
            }

            pthread_create(&worker->tid, NULL, 
                    conet::ptr_cast<void *(*)(void*)>(&ServerWorker::proc), worker);
        }

        return 0;
    }

    int run()
    {
        while (!m_stop_flag) 
        {
            sleep(1);
        }
        return 0;
    }

    int stop(int wait_seconds)
    {
        int ret = 0;
        ret = ServerController::stop(wait_seconds);

        for (size_t i=0; i< m_workers.size(); ++i)
        {
            m_workers[i]->stop(wait_seconds);
        }

        for (size_t i=0; i< m_workers.size(); ++i)
        {
            pthread_join(m_workers[i]->tid, NULL);
        }
        m_stop_finished = 1;
        return 0;
    }

};

ServerController * ServerController::create()
{
    ServerController * server = NULL;
    if (is_thread_mode()) {
        server =  new ServerControllerThreadMode();
    } else {
        server =  new ServerControllerProcessMode();
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

int ServerController::start()
{
    return 0;
}

int ServerController::run()
{
    return 0;
}

int ServerController::stop(int wait_seconds)
{
    m_stop_flag = 1;
    for (size_t i=0; i<m_workers.size(); ++i)
    {
        m_workers[i]->m_exit_flag = 1;
    }
    return 0;
}

}
