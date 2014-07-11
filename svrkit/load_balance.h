/*
 * =====================================================================================
 *
 *       Filename:  load_balance.h
 *
 *    Description:  
 *
 *        Version:  1.0
 *        Created:  2014年07月11日 17时25分04秒
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

#include "ip_list.h"
#include "core/incl/auto_var.h"
#include "net_tool.h"
#include <tr1/unordered_map>

namespace conet
{

class FdPool
{
public:
    class Node
    {
    public:
        ip_port_t ip_port;
        std::queue<int> m_fds;
    };

    std::tr1::unordered_map<ip_port_t, Node> m_nodes;

    FdPool()
    {

    }


    int get(char const *ip, int port)
    {
        ip_port_t key;
        key.ip = ip;
        key.port = port;

        return this->get(key);
    }

    int get(ip_port_t const & key) 
    {

        AUTO_VAR(it, =, m_nodes.find(key));
        if (it == m_nodes.end()) {
            Node &node = m_nodes[key];
            node.ip_port = key;
            it = m_nodes.find(key);
        }
        Node & node = it->second;

        int fd = -1;
        if (node.m_fds.empty()) {
            fd = connect_to(node.ip_port.ip.c_str(), node.ip_port.port);
            return fd;
        }

        fd = node.m_fds.front();
        node.m_fds.pop();
        return fd;
    }

    void add(char const *ip, int port, int fd) 
    {
        ip_port_t key;
        key.ip = ip;
        key.port = port;
        return this->add(key, fd);
    }

    void add(ip_port_t const & key, int fd) 
    {

        AUTO_VAR(it, =, m_nodes.find(key));
        if (it == m_nodes.end()) {
            Node &node = m_nodes[key];
            node.ip_port = key;
            it = m_nodes.find(key);
        }
        Node & node = it->second;

        node.m_fds.push(fd);
        return;
    }

private:
    FdPool(FdPool &rval) ;

};

class IpListLB
{
public: 

    class Node
    {
    public:
        ip_port_t ip_port;

        int weigth;
        int would_cnt;
        uint64_t called;
        uint64_t success_called;
        uint64_t failed_called;
        list_head link_to;

        int calc() 
        {
            if (weigth == 0) {
                weigth = 100;
                return 0;
            }
            if (called == 0) {
                weigth = 100;
                return 0;
            }

            weigth = success_called * 100 / called; 

            return 0;

        }

        Node()
        {
            INIT_LIST_HEAD(&link_to);
            called = 0;
            success_called = 0;
            failed_called = 0;
            weigth = 0;
            would_cnt = 0;
        }
    };

    std::vector<Node *> m_nodes;
    std::tr1::unordered_map<ip_port_t, Node *> m_ip_port_nodes;

    FdPool m_fds;

    uint32_t m_pick_num;

    int init(std::string const &ips) 
    {
        m_pick_num = 0;

        std::vector<ip_port_t> list;
        parse_ip_list(ips, &list);
        if (list.empty()) {
            return -1;
        }

        for(int i=0, len = (int)list.size(); i<len; ++i)
        {
            Node *node = new Node();
            node->ip_port = list[i];
            m_nodes.push_back(node);

            m_ip_port_nodes[list[i]] = node;
        }
        return 0;
    }


    std::vector<Node *> m_schedule_list;
    int m_schedule_pos;

    int calc() 
    {
        int sum = 0;
        for(int i=0, len =  (int) m_nodes.size(); i<len; ++i)
        {
            m_nodes[i]->calc();
            sum += m_nodes[i]->weigth;
        }

        list_head schedule;
        INIT_LIST_HEAD(&schedule);

        for(int i=0, len =  (int) m_nodes.size(); i<len; ++i)
        {
            m_nodes[i]->would_cnt = m_nodes[i]->weigth * 101 / sum;
            if (m_nodes[i]->would_cnt <= 0) m_nodes[i]->would_cnt = 1;
            list_add_tail(&m_nodes[i]->link_to, &schedule);
        }


        m_schedule_list.clear();
        m_schedule_pos = 0;

        while(1)  {
            list_head *it=NULL, *next=NULL;
            int cnt = 0;
            list_for_each_safe(it, next, &schedule) 
            {
                Node *n = container_of(it, Node, link_to);

                if (n->would_cnt > 0) {
                    m_schedule_list.push_back(n);
                    --(n->would_cnt); 
                } 

                if (n->would_cnt <=0) {
                    list_del_init(it);
                }
            }
            if (cnt == 0) {
                break;
            }
        }
        return 0;
    }


    int get(std::string *ip, int *port) 
    {
        ip_port_t ip_port;
        int fd = -1;
        fd = this->get(&ip_port);
        *ip = ip_port.ip;
        *port= ip_port.port;
        return fd;
    }

    int get(ip_port_t * ip_port)
    {
        int fd = -1;
        Node *n = NULL;
        for (int i=0; i<10; ++i) 
        {
            if (m_schedule_pos >= (int)m_schedule_list.size()) {
                calc();
            }

            n = m_schedule_list[m_schedule_pos];
            ++m_schedule_pos;

            fd = m_fds.get(n->ip_port.ip.c_str(), n->ip_port.port);
            if (fd >= 0) {
                *ip_port = n->ip_port;
                break;
            }

            ++ n->called;
            ++ n->failed_called;
        }

        return fd;
    }

    void release(char const *ip, int port, int fd, int status = 0) 
    {
        ip_port_t ip_port;
        ip_port.ip = ip;
        ip_port.port = port;
        return this->release(ip_port, fd, status);
    }

    void release(ip_port_t const &ip_port, int fd, int status) 
    {
        AUTO_VAR(it, =, m_ip_port_nodes.find(ip_port));
        if (it != m_ip_port_nodes.end()) {
            ++it->second->called;
        }
        if (status == 0) {
            m_fds.add(ip_port, fd);
            it->second->success_called;
           
        } else {
            close(fd);
            it->second->failed_called;
        }
    }

    IpListLB()
    {
    }

    ~IpListLB()
    {
        for(typeof(m_nodes.begin()) it = m_nodes.begin(), iend = m_nodes.end(); 
                it!=iend; ++it)
        {
            delete *it;
        }
    }

private:
    IpListLB(IpListLB &rval);
};

}

#endif /* end of include guard */
