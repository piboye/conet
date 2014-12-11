/*
 * =====================================================================================
 *
 *       Filename:  server_container.cpp
 *
 *    Description:  
 *
 *        Version:  1.0
 *        Created:  2014年12月09日 16时41分40秒
 *       Revision:  none
 *       Compiler:  gcc
 *
 *         Author:  piboye
 *   Organization:  
 *
 * =====================================================================================
 */
#include <stdlib.h>

struct ServerContainer
{

        std::vector<server_base_t *> m_servers;



        int init(TaskEnv *env);

        int start()
        {

        }


        int stop(int timeout)
        {
            int ret = 0;
            ret = rpc_server.stop(timeout);
            return ret;
        }

};

