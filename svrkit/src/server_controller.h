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


    ServerController();

    virtual
    ~ServerController();

    virtual
    int start()
    {
        int ret=0;
        return ret;
    }

    virtual
    int stop();


    int run();

    static ServerController * create();

};

}

#endif /* end of include guard */
