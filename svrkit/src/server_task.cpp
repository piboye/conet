/*
 * =====================================================================================
 *
 *       Filename:  server_task.cpp
 *
 *    Description:  
 *
 *        Version:  1.0
 *        Created:  2014年12月11日 19时35分19秒
 *       Revision:  none
 *       Compiler:  gcc
 *
 *         Author:  piboye
 *   Organization:  
 *
 * =====================================================================================
 */
#include <stdlib.h>
#include "server_task.h"
#include <vector>

namespace conet
{

    static std::vector<ServerTask*> *g_server_task = NULL;

    int ServerTask::add_task(ServerTask *task)
    {

        if (NULL == g_server_task)
        {
            g_server_task = new std::vector<ServerTask *>();
        }
        g_server_task->push_back(task);
        return 0;
    }

    int ServerTask::get_all_task(std::vector<ServerTask *> * tasks)
    {
        for (size_t i =0; i< g_server_task->size(); ++i)
        {
            tasks->push_back( g_server_task->at(i)->clone());
        }
        return 0;
    }

    
    __attribute__ ((destructor))
    static
    void delete_server_task();

    static
    void delete_server_task()
    {
        if (NULL == g_server_task) {
           return;
        }
        for (size_t i =0; i< g_server_task->size(); ++i)
        {
            delete g_server_task->at(i);
        }
        delete g_server_task;
    }

}


