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

namespace conet
{

server_worker_t::server_worker_t()
{
    tid = 0;
    server_group = NULL;
    stop_flag = 0;
    exit_finsished = 0;
    exit_seconds = 0;
}

void* server_worker_t::main(void * arg)
{
    server_worker_t *self = (server_worker_t *)(arg);
    server_group_t * server_group = self->server_group;
    ServerGroup conf;
    conf.CopyFrom(server_group->conf_data);
    int size = conf.servers_size();

    for (int i=0; i<size; ++i)
    {
        RpcServer const & server_conf = conf.servers(i);
        rpc_pb_server_t *rpc_server =  server_group->build_rpc_server(server_conf);
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
    int thread_num = conf.thread_num();
    if (thread_num <=0) {
        thread_num  = sysconf(_SC_NPROCESSORS_ONLN);
        if (thread_num <=0) thread_num=1;
    }
    server->thread_num  = thread_num;
    
    return server;
}

int server_group_t::start()
{
    server_worker_t * worker = NULL;
    int num = thread_num;
    for (int i = 0; i< num ; ++i)
    {
        worker = new server_worker_t();
        worker->server_group = this;
        pthread_create(&worker->tid, NULL,
                server_worker_t::main, worker);
        m_work_pool.push_back(worker);
    }
    return 0;
}

rpc_pb_server_t *server_group_t::build_rpc_server(RpcServer const & conf)
{
    rpc_pb_server_t * server = new rpc_pb_server_t();
    std::string server_name = conf.server_name();
    rpc_pb_server_base_t * server_base = new rpc_pb_server_base_t();

    int ret = 0;
    ret = server_base->get_global_server_cmd(server_name);
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
        ret = server->add_server(tcp_server);
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

