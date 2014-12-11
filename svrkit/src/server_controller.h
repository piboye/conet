/*
 * =====================================================================================
 *
 *       Filename:  server_controller.h
 *
 *    Description:  
 *
 *        Version:  1.0
 *        Created:  2014年12月11日 14时10分54秒
 *       Revision:  none
 *       Compiler:  gcc
 *
 *         Author:  piboye
 *   Organization:  
 *
 * =====================================================================================
 */
#ifndef __CONET_SERVER_CONTROLLER_H__
#define __CONET_SERVER_CONTROLLER_H__


#include <stdlib.h>
#include <string>
#include <vector>
#include <map>

namespace conet
{

class ServerWorker;

class ServerController
{
public:

    int m_worker_mode;
    int m_curr_num; // 并发数
    int m_stop_flag; // 停止标志

    std::vector<ServerWorker*> m_workers;
    std::map<int, ServerWorker*> m_worker_map;
    std::vector<int> m_cpu_set;


    ServerController()
    {
        m_worker_mode = 0;
        m_stop_flag = 0;
        m_curr_num = 1;
    }

    virtual
    ~ServerController();

    int set_curr_num(int num)
    {
        m_curr_num = num;
        return 0;
    }

    virtual
    int start()
    {
        int ret=0;
        return ret;
    }

    virtual
    int stop();


    int run();

    static ServerController * create(int thread_mode = 0);

};

}

#endif /* end of include guard */
