/*
 * =====================================================================================
 *
 *       Filename:  server_base.h
 *
 *    Description:  
 *
 *        Version:  1.0
 *        Created:  2014年11月17日 06时10分07秒
 *       Revision:  none
 *       Compiler:  gcc
 *
 *         Author:  piboye
 *   Organization:  
 *
 * =====================================================================================
 */
#ifndef __CONET_SERVER_BASE_H__
#define __CONET_SERVER_BASE_H__

namespace conet
{
    struct ServerBase
    {
        enum {
            SERVER_START=0,
            SERVER_RUNNING=1,
            SERVER_STOPED=2,
        };

        std::string m_server_name;

        virtual int start()=0;

        virtual int stop(int wait_ms)=0;

        virtual ~ServerBase() 
        {

        }
    };


    struct ServerCombine :public ServerBase
    {
        ServerBase * m_main_server;
        std::vector<ServerBase *> m_servers;

        ServerCombine()
        {
            m_main_server = NULL;
        }

        int init(ServerBase * main_server)
        {
            m_main_server = main_server;
            return 0;
        }

        int add(ServerBase *server)
        {
            m_servers.push_back(server);
            return 0;
        }

        virtual int start()
        {
            return m_main_server->start();
        }

        virtual int stop(int wait_ms)
        {
            return m_main_server->stop(wait_ms);
        }

        virtual ~ServerCombine() 
        {
            for(size_t i =0; i<m_servers.size(); ++i)
            {
                delete m_servers[i];
            }
            delete m_main_server;
        }
    };
}


#endif /* end of include guard */
