/*
 * =====================================================================================
 *
 *       Filename:  server_task.h
 *
 *    Description:  
 *
 *        Version:  1.0
 *        Created:  2014年12月11日 19时35分38秒
 *       Revision:  none
 *       Compiler:  gcc
 *
 *         Author:  piboye
 *   Organization:  
 *
 * =====================================================================================
 */

#ifndef __CONET_SERVER_TASK_H__
#define __CONET_SERVER_TASK_H__

#include <vector>

namespace conet
{

class ServerTask
{

public:

    virtual
    int start()
    {
        return 0;
    }

    virtual
    int stop(int wait_ms)
    {
        return 0;
    }

    virtual ~ServerTask()
    {

    }

    virtual
    ServerTask * clone() { return NULL;};

    typedef ServerTask * generator_fun_t();

    static 
    int add_task(ServerTask *task);

    static 
    int get_all_task(std::vector<ServerTask *> * tasks);

};

}


#endif /* end of include guard */
