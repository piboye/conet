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

#include <string>
#include <vector>

namespace conet
{
    struct server_base_t
    {
        enum {
            SERVER_START=0,
            SERVER_RUNNING=1,
            SERVER_STOPING=2,
            SERVER_STOPED=3,
        };
        int to_stop;

        int state;

        std::string m_server_name;

        server_base_t()
        {
            state = SERVER_START; 
            to_stop = 0;
        }

        virtual int start()=0;

        int stop(int wait_ms=0)
        {
            this->to_stop = 1;
            if (this->state == SERVER_STOPED ||
                    this->state == SERVER_STOPING) {
                return 0;
            }
            this->state = SERVER_STOPING;
            return this->do_stop(wait_ms);
        }

        virtual int do_stop(int wait_ms) = 0;

        virtual ~server_base_t() 
        {

        }
    };


    struct server_combine_t :public server_base_t
    {
        server_base_t * m_main_server;
        std::vector<server_base_t *> m_servers;

        server_combine_t()
        {
            m_main_server = NULL;
        }

        int init(server_base_t * main_server)
        {
            m_main_server = main_server;
            return 0;
        }

        int add(server_base_t *server)
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

        virtual ~server_combine_t() 
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
