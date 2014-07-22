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
 *         Author:  piboyeliu
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
#include "core/incl/time_helper.h"
#include "net_tool.h"
#include <tr1/unordered_map>
#include <math.h>
#include <map>

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

    std::map<ip_port_t, Node> m_nodes;

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

        int dymanic_weight;
        int would_cnt;
        uint64_t called;
        uint64_t success_called;
        uint64_t failed_called;

        uint64_t success_tk; // cpu tick
        uint64_t failed_tk;  // cpu tick

        uint64_t avg_cost; // cpu tick

        list_head link_to;
        int succ_rate;



        int calc() 
        {
            if (dymanic_weight == 0) {
                dymanic_weight = 100;
                return 0;
            }



            int t = success_called;// - 5*failed_called;
            if (t <=0) t = 1;

            int s = this->called;  
            if (s == 0) {
                s=1;
            }

            avg_cost = (success_tk) / s;

            if (avg_cost <= 0) avg_cost = 1;

             t  = t *100/ s;
             succ_rate = t;

            if (t >=90) {
                dymanic_weight = success_called;
            } else {
                dymanic_weight = success_called;
            }
            if (this->dymanic_weight <= 0) {
                this->dymanic_weight = 1;
            }
            

            return 0;

        }

        int reinit_stat()
        {
            avg_cost = 0;
            called = 0;
            success_called = 0;
            failed_called = 0;
            success_tk = 0;
            failed_tk =0;
            succ_rate = 0;
            return 0;
        }

        Node()
        {
            INIT_LIST_HEAD(&link_to);
            called = 0;
            success_called = 0;
            failed_called = 0;
            dymanic_weight = 0;
            avg_cost = 1;
            would_cnt = 0;
            succ_rate = 0;
        }
    };

    std::vector<Node *> m_nodes;
    std::map<ip_port_t, Node *> m_ip_port_nodes;

    FdPool m_fds;

    std::vector<Node *> m_schedule_list;
    int m_schedule_pos;
    std::tr1::unordered_map<int, uint64_t> m_fd_start_tks;


    int init(std::string const &ips) 
    {

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
        m_report_num=0;
        return 0;
    }



    int calc() 
    {
        int sum = 0;
        uint64_t avg_cost = 0;
        uint64_t sum_tk = 0;
        int sum_cnt = 0;

        for(int i=0, len =  (int) m_nodes.size(); i<len; ++i)
        {
            Node *n = m_nodes[i];
            n->calc();
            sum += n->dymanic_weight;
            sum_tk += n->success_tk;
            sum_cnt += n->success_called;
        }

        if (sum_cnt == 0) sum_cnt = 1;
        
        int avg_wg = sum / m_nodes.size();

        avg_cost = sum_tk / sum_cnt;
        if (avg_cost <=0) avg_cost = 1;

        list_head schedule;
        INIT_LIST_HEAD(&schedule);

        for(int i=0, len =  (int) m_nodes.size(); i<len; ++i)
        {
            Node *n = m_nodes[i];

            float prio = (float)(avg_cost - n->avg_cost)/avg_cost;
            if (prio > 1) prio = 1;
            if (prio <= -1) prio = -1;

            if (n->succ_rate > 98 && (n->dymanic_weight < avg_wg)) {
                n->would_cnt = (n->dymanic_weight*2 * 101 / sum); 
            } else {
                n->would_cnt = (n->dymanic_weight * 101 / sum);
            }
            if (n->would_cnt <= 0) n->would_cnt = 1;
            n->reinit_stat();

            list_del_init(&m_nodes[i]->link_to);
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
                    ++cnt;
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

    int m_report_num;

    int get(ip_port_t * ip_port)
    {
        int fd = -1;
        Node *n = NULL;
        for (int i=0; i<10; ++i) 
        {
            if (m_report_num >= (int)m_schedule_list.size()) {
                calc();
                m_report_num = 0;
            }

            int pos = (m_schedule_pos++)%m_schedule_list.size();
            

            n = m_schedule_list[pos];

            uint64_t start_tk = rdtscp();
            fd = m_fds.get(n->ip_port.ip.c_str(), n->ip_port.port);
            if (fd >= 0) {
                m_fd_start_tks[fd] = start_tk;
                *ip_port = n->ip_port;
                break;
            }

            ++m_report_num;
            n->failed_tk += (rdtscp()-start_tk); 
            ++ n->called;
            ++ n->failed_called;
        }
        if (fd <0) *ip_port = n->ip_port;
        //fprintf(stderr, "select fd:%d, address:[%s:%d]\n", fd, ip_port->ip.c_str(), ip_port->port);

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
        if (it == m_ip_port_nodes.end()) {
            if (fd>=0) {
                close(fd);
            }
            return;
        }

        ++m_report_num;

        Node *n = it->second;
        ++n->called;

        uint64_t tk = m_fd_start_tks[fd];
        if (tk >0) {
            tk = rdtscp() - tk;
        } else {
            tk = 1;
        }

        if (status == 0) {
            m_fds.add(ip_port, fd);
            ++n->success_called;
            n->success_tk += tk;   
        } else {
            close(fd);
            ++n->failed_called;
            n->failed_tk += tk;
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
