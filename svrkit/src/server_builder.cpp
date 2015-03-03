/*
 * =====================================================================================
 *
 *       Filename:  server_builder.cpp
 *
 *    Description:  
 *
 *        Version:  1.0
 *        Created:  2015年02月17日 02时45分28秒
 *       Revision:  none
 *       Compiler:  gcc
 *
 *         Author:  piboye
 *   Organization:  
 *
 * =====================================================================================
 */
#include <stdlib.h>
#include "server_base.h"
#include "rpc_pb_server.h"
#include "svrkit/rpc_conf.pb.h"
#include "server_builder.h"
#include "server_common.h"
#include <pthread.h>
#include "base/cpu_affinity.h"
#include <sched.h>

namespace conet
{

server_worker_t::server_worker_t()
{
    tid = 0;
    server_group = NULL;
    stop_flag = 0;
    exit_finsished = 0;
    exit_seconds = 0;
    cpu_affinity = NULL;
}

void* server_worker_t::main(void * arg)
{
    server_worker_t *self = (server_worker_t *)(arg);

    if (self->cpu_affinity && CPU_COUNT(self->cpu_affinity) > 0) {
        int ret = 0;
        pthread_t tid = pthread_self();
        ret = pthread_setaffinity_np(tid, sizeof(cpu_set_t), self->cpu_affinity);
        if (ret) {
            LOG(ERROR)<<"set tid:"<<tid<<" to  cpu:"<<" affinity failed, ret:"<<ret;
        } else {
            LOG(INFO)<<"set tid:"<<tid<<" to  cpu:"<<" affinity success";
        }
    }

    int size = self->conf.servers_size();

    for (int i=0; i<size; ++i)
    {
        RpcServer const & server_conf = self->conf.servers(i);
        rpc_pb_server_t *rpc_server =  self->build_rpc_server(server_conf);
        if (rpc_server) {
            self->rpc_servers.push_back(rpc_server);
        }
    }

    int ret = 0;
    size = self->rpc_servers.size();
    for (int i=0; i<size; ++i)
    { // 启动server
        ret = self->rpc_servers[i]->start();
        if (ret)
        {
            LOG(ERROR)<<"error start";
        }
    }

      coroutine_t *exit_co = NULL;

      while (likely(!self->exit_finsished)) 
      {
          if (unlikely(get_server_stop_flag() && exit_co == NULL)) 
          {
              exit_co = conet::alloc_coroutine(
                      conet::ptr_cast<conet::co_main_func_t>(
                          &server_worker_t::proc_server_exit), self);
              conet::resume(exit_co);
          }
          conet::dispatch();
      }

      if (exit_co) 
      {
          free_coroutine(exit_co);
      }
    return NULL;
}

int server_worker_t::proc_server_exit()
{
    int ret = 0;
    for(size_t i=0; i< rpc_servers.size(); ++i)
    {
        ret = rpc_servers[i]->stop(exit_seconds);
        if (ret)
        {
            LOG(ERROR)<<"stop rpc server failed! ";
        }
    }
    exit_finsished = 1;
    return 0;
}

server_group_t::server_group_t()
{
    stop_flag = 0;
}

server_group_t * server_group_t::build(ServerGroup const &conf)
{
    server_group_t * server = new server_group_t();
    server->group_name = conf.group_name();
    server->conf_data.CopyFrom(conf);

    int cpu_num = sysconf(_SC_NPROCESSORS_ONLN);
    int thread_num = conf.thread_num();
    if (thread_num <=0) {
        thread_num  = cpu_num; 
        if (thread_num <=0) thread_num=1;
    }

    server->thread_num  = thread_num;
    int ret =  0;

    for(int i = 0; i<conf.cpu_affinitys_size(); ++i)
    {
        std::string txt = conf.cpu_affinitys(i);
        cpu_set_t mask;
        CPU_ZERO(&mask);
        ret = parse_affinity(txt.c_str(), &mask);
        if (ret >= 0) { 
            server->cpu_affinitys.push_back(mask);
        } else {
            // 格式有问题
            LOG(FATAL)<<"cpu mask error format :"<<txt;
            abort();
        }
    }
    return server;
}

int server_group_t::start()
{
    server_worker_t * worker = NULL;
    int num = thread_num;
    int cpu_len = cpu_affinitys.size();
    for (int i = 0; i< num ; ++i)
    {
        worker = new server_worker_t();
        worker->server_group = this;

        if (cpu_len >0)
        {
            worker->cpu_affinity = &cpu_affinitys[i%cpu_len];
        }

        worker->conf.CopyFrom(this->conf_data);
        pthread_create(&worker->tid, NULL,
                server_worker_t::main, worker);
        m_work_pool.push_back(worker);
    }
    return 0;
}

static pthread_mutex_t g_server_work_mutex=PTHREAD_MUTEX_INITIALIZER;

rpc_pb_server_t *server_worker_t::build_rpc_server(RpcServer const & conf)
{
    rpc_pb_server_t * server = new rpc_pb_server_t();
    std::string server_name = conf.server_name();
    rpc_pb_server_base_t * server_base = new rpc_pb_server_base_t();

    int ret = 0;
    pthread_mutex_lock(&g_server_work_mutex);
    ret = server_base->get_global_server_cmd(server_name);
    pthread_mutex_unlock(&g_server_work_mutex);
    if (ret<0)
    {
        LOG(ERROR)<<"find [server-name:"<<server_name<<"] failed!";
        delete server;
        delete server_base;
        return NULL;
    }
    server->init(server_base);

    int can_reuse_port_flag = conet::can_reuse_port();
    // tcp_server
    for(int i=0; i< conf.tcp_server_size(); ++i)
    {
        tcp_server_t * tcp_server = new tcp_server_t();
        TcpServer const &tcp_conf = conf.tcp_server(i);
        if (can_reuse_port_flag) {
            ret = tcp_server->init(tcp_conf.ip().c_str(), tcp_conf.port());
        } else {
            int fd = get_listen_fd_from_pool(tcp_conf.ip().c_str(), tcp_conf.port());
            if (fd >=0) {
                ret = tcp_server->init(tcp_conf.ip().c_str(), tcp_conf.port(), fd);
            } else {
                ret = -1;
            }
        }
        if (ret)  {
            delete tcp_server;
            LOG(FATAL)<<"listen ["<<tcp_conf.ip()<<":"<<tcp_conf.port()<<"failed!";
            return NULL;
        }
        tcp_server->conf.max_conn_num = tcp_conf.max_conn_num();
        tcp_server->conf.duplex = tcp_conf.duplex();

        ret = server->add_server(tcp_server);
        // 开启http 接口
        if (tcp_conf.enable_http()) {
            http_server_t *http_server = new http_server_t();
            http_server->tcp_server = tcp_server;
            tcp_server->extend = http_server;
            ret = server->add_server(http_server);
        }
    }

    // udp_server
    for(int i=0; i< conf.udp_server_size(); ++i)
    {
        udp_server_t * udp_server = new udp_server_t();
        UdpServer const &udp_conf = conf.udp_server(i);
        if (can_reuse_port_flag) {
            ret = udp_server->init(udp_conf.ip().c_str(), udp_conf.port());
        } else {
            int fd = get_listen_fd_from_pool(udp_conf.ip().c_str(), udp_conf.port());
            if (fd >=0) {
                ret = udp_server->init(udp_conf.ip().c_str(), udp_conf.port(), fd);
            } else {
                ret = -1;
            }
        }
        if (ret)  {
            delete udp_server;
            LOG(FATAL)<<"listen ["<<udp_conf.ip()<<":"<<udp_conf.port()<<"failed!";
            return NULL;
        }
        udp_server->conf.max_conn_num = udp_conf.max_conn_num();
        ret = server->add_server(udp_server);
    }
    // http_server
    for(int i=0; i< conf.http_server_size(); ++i)
    {
        tcp_server_t * tcp_server = new tcp_server_t();
        HttpServer const &http_conf = conf.http_server(i);
        std::string ip = http_conf.ip();
        int port = http_conf.port();
        if (can_reuse_port_flag) {
            ret = tcp_server->init(ip.c_str(), port);
        } else {
            int fd = get_listen_fd_from_pool(ip.c_str(), port);
            if (fd >=0) {
                ret = tcp_server->init(ip.c_str(), port, fd);
            } else {
                ret = -1;
            }
        }
        if (ret)  {
            delete tcp_server;
            LOG(FATAL)<<"listen ["<<ip<<":"<<port<<"failed!";
            return NULL;
        }
        tcp_server->conf.max_conn_num = http_conf.max_conn_num();

        http_server_t *http_server = new http_server_t();
        http_server->init(tcp_server);
        ret = server->add_server(http_server);
    }

    return server;
}

int server_worker_t::stop(int seconds)
{
    int ret = 0;
    stop_flag = 1;
    exit_seconds = seconds;
    void * exit_status=0;
    pthread_join(tid, &exit_status);
    return ret;
}

int server_group_t::stop(int seconds)
{
    stop_flag = 1;
    for (size_t i =0; i< m_work_pool.size(); ++i)
    {
        m_work_pool[i]->stop(seconds);
    }
    return 0;
}

ServerContainer *ServerBuilder::build(ServerConf const &conf)
{
    ServerContainer *container = new ServerContainer();
    for(int i=0; i< conf.server_groups_size(); ++i)
    {
        server_group_t *group = server_group_t::build(conf.server_groups(i));
        container->server_groups.push_back(group);
    }
    return container;
}

int ServerContainer::start()
{
    int ret = 0;
    for(size_t i= 0; i<server_groups.size(); ++i) 
    {
       ret = server_groups[i]->start(); 
       if (ret)
       {
           break;
       }
    }
    return ret;
}

int ServerContainer::stop(int seconds)
{
    for(size_t i= 0; i<server_groups.size(); ++i) 
    {
        server_groups[i]->stop_flag = 1;
    }

    for(size_t i= 0; i<server_groups.size(); ++i) 
    {
       server_groups[i]->stop(seconds); 
    }
    return 0;
}

}

