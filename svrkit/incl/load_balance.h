/*
 * =====================================================================================
 *
 *       Filename:  load_balance2.h
 *
 *    Description:  
 *
 *        Version:  1.0
 *        Created:  09/12/2014 07:50:06 AM
 *       Revision:  none
 *       Compiler:  gcc
 *
 *         Author:  YOUR NAME (), 
 *   Organization:  
 *
 * =====================================================================================
 */

#ifndef LOAD_BALANCE_H_INC_
#define LOAD_BALANCE_H_INC_

#include <vector>
#include <queue>
#include <list>
#include <string>
#include <algorithm>
#include <math.h>

#include "base/incl/net_tool.h"
#include "base/incl/time_helper.h"
#include "base/incl/ip_list.h"
#include "base/incl/auto_var.h"
#include "base/incl/time_helper.h"

#include "base/incl/random.h"


namespace conet
{

class FdPool
{
public:
    std::queue<int> m_fds;
    std::vector<int> m_calls;
    Rand m_rand;

    FdPool()
    {
        m_rand.reseed(rdtscp());
    }

    int get() 
    {
        if (m_fds.empty()) {
            return -1;
        }

        int fd = m_fds.front();
        m_fds.pop();
        return fd;
    }

    int put(int fd) 
    {
        if ((int)m_calls.size() <=fd)
        {
            m_calls.resize(fd+1000);
        }

        if (m_calls[fd] <= 0) {
            m_calls[fd] = 10000 + m_rand.rand_u32()%10000;
        }

        --m_calls[fd];
        if (m_calls[fd] <=0) {
            //close(fd);
            //return 0;
        }
        m_fds.push(fd);
        return 0;
    }
};

class IpListConf
{
public:

    std::vector<ip_port_t> m_hosts;
    int m_pos;
    Rand m_rand;

    IpListConf()
    {
        m_pos = 0;
        m_rand.reseed(rdtscp());
    }

    explicit
    IpListConf(char const *src)
    {
        m_rand.reseed(rdtscp());
        parse_ip_list(src, &m_hosts); 
        if (m_hosts.size() > 0) {
            m_pos = m_rand.rand_u32()%m_hosts.size();
        }
    }

    int reload(char const *src)
    {
        m_hosts.clear();
        parse_ip_list(src, &m_hosts); 
        if (m_hosts.size() > 0) {
            m_pos = m_rand.rand_u32()%m_hosts.size();
        }
        return 0;
    }

    int add(char const *ip, int port)
    {
        ip_port_t host;
        host.ip = ip;
        host.port = port;
        m_hosts.push_back(host);
        return 0;
    }

    int remove(char const *ip, int port)
    {
        ip_port_t host;
        host.ip = ip;
        host.port = port;
        m_hosts.erase(
                std::remove(m_hosts.begin(), m_hosts.end(), host), 
                m_hosts.end()
                );

        return 0;
    }

    ip_port_t * get_node() 
    {
        if (m_hosts.empty()) {
            return NULL;
        }

        m_pos = m_rand.rand_u32() % m_hosts.size();
        ip_port_t *host = &m_hosts[m_pos];
        return host;
    }

};

class IpListLB
{
public:
    FdPool m_fd_pool;

    IpListConf m_ip_lists;


    IpListLB()
    {
    }

    int init(char const * src) 
    {
        m_ip_lists.reload(src);
        return 0;
    }

    int init(std::string const & src) 
    {
        return init(src.c_str());
    }

    int get(std::string *ip=NULL, int *port=NULL)
    {
        int fd = 0;
        fd = m_fd_pool.get();
        if (fd <0) {
            for (int i=0; i<3; ++i) {
                ip_port_t * host = m_ip_lists.get_node();
                fd = connect_to(host->ip.c_str(), host->port);
                if (fd >=0)  {
                    set_nodelay(fd);
                    if (ip) *ip = host->ip;
                    if (port) *port = host->port;
                    break;
                }
            }
        } else {
            //*ip = host->ip;
            //*port = host->port;

        }
        return fd;
    }

    int release(int fd, int status , std::string *ip=NULL, int *port=NULL)
    {
        if (status != 0) {
            close(fd);
        } else {
            m_fd_pool.put(fd);
        }
        return 0;
    }
};

}


#endif /* end of include guard */
