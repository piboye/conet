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
        std::string m_server_name;
        virtual int start()=0;

        virtual int stop(int wait_ms)=0;

        virtual ~ServerBase() 
        {

        }
    };


    struct ServerCombine :public ServerBase
    {
        std::vector<ServerBase *> m_servers;

        int add(ServerBase *server)
        {
            m_servers.push_back(server);
            return 0;
        }

        virtual int start()
        {
            int ret = 0, ret2 = 0;
            for(size_t i =0; i<m_servers.size(); ++i)
            {
                ret = m_servers[i]->start();
                if (ret) {
                    ret2 -=1;
                }
            }
            return ret2;
        }

        virtual int stop(int wait_ms)
        {
            int ret = 0, ret2 = 0;
            for(size_t i =0; i<m_servers.size(); ++i)
            {
                ret = m_servers[i]->stop(wait_ms);
                if (ret) {
                    ret2 -=1;
                }
            }
            return ret2;
        }

        virtual ~ServerCombine() 
        {
            for(size_t i =0; i<m_servers.size(); ++i)
            {
                delete m_servers[i];
            }
            m_servers.clear();
        }
    };
}


#endif /* end of include guard */
