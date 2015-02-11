/*
 * =====================================================================================
 *
 *       Filename:  rpc_pb_server_task.cpp
 *
 *    Description:  组装server
 *
 *        Version:  1.0
 *        Created:  2014年12月12日 06时11分53秒
 *       Revision:  none
 *       Compiler:  gcc
 *
 *         Author:  piboye
 *   Organization:  
 *
 * =====================================================================================
 */
#include <stdlib.h>
#include "server_task.h"
#include "base/delay_init.h"

#include "async_rpc_pb_server.h"
#include "rpc_pb_server.h"
#include "rpc_pb_server_base.h"

#include "base/ip_list.h"
#include "server_common.h"

namespace conet
{
DEFINE_string(server_address, "0.0.0.0:12314", "default server address");

static conet::rpc_pb_server_base_t g_rpc_pb_server_base;    

class RpcPbServerTask
    : public ServerTask
{
public:
    conet::tcp_server_t tcp_server;    
    conet::http_server_t http_server;    
    conet::rpc_pb_server_t rpc_server;    

    ip_port_t rpc_ip_port;

    int rpc_listen_fd;

    int init()
    {
        int ret = 0;
        ret = tcp_server.init(rpc_ip_port.ip.c_str(), rpc_ip_port.port, rpc_listen_fd);
        ret = http_server.init(&tcp_server);
        ret = rpc_server.init(&g_rpc_pb_server_base);
        ret = rpc_server.add_server(&tcp_server);
        ret = rpc_server.add_server(&http_server);

        return ret;
    }

    ServerTask *clone()
    {
        RpcPbServerTask * task = new RpcPbServerTask(*this);
        task->rpc_listen_fd = get_listen_fd(this->rpc_ip_port.ip.c_str(), this->rpc_ip_port.port, this->rpc_listen_fd);
        task->init();
        return task;
    }

    int start()
    {
        rpc_server.start();
        if (tcp_server.state == tcp_server_t::SERVER_STOPED) {
            LOG(ERROR)<<"listen to ["<<rpc_ip_port.ip.c_str()<<":"<<rpc_ip_port.port<<"], failed"; 
            return 0;
        }
        fprintf(stderr, "listen to [%s:%d]\n, success\n", rpc_ip_port.ip.c_str(), rpc_ip_port.port);
        LOG(INFO)<<"listen to ["<<rpc_ip_port.ip.c_str()<<":"<<rpc_ip_port.port<<"], success"; 
        return 0;
    }

    int stop(int timeout)
    {
        int ret = 0;
        ret = rpc_server.stop(timeout);
        return ret;
    }

};


DELAY_INIT()
{
    int ret = 0;
    ret = g_rpc_pb_server_base.get_global_server_cmd("main_server");

    if (!FLAGS_server_address.empty())
    {

        std::vector<ip_port_t> rpc_ip_list;
        parse_ip_list(FLAGS_server_address, &rpc_ip_list);
        for(size_t i=0; i< rpc_ip_list.size(); ++i)
        {
            RpcPbServerTask * task = new RpcPbServerTask();
            ip_port_t ip_port = rpc_ip_list[i];
            task->rpc_ip_port = ip_port;
            task->rpc_listen_fd = conet::create_tcp_socket(ip_port.port, ip_port.ip.c_str(), true);
            if (task->rpc_listen_fd < 0) {
                LOG(ERROR)<<"create listen fd failed! [errno:"<<errno<<"][errmsg:"<<strerror(errno)<<"]";
                LOG(ERROR)<<"listen address "<<ip_port.ip<<":"<<ip_port.port<<" maybe used by other program, please check";
                delete task;
                exit(1);
            } else {
                ServerTask::add_task(task);
            }
        }
    }

    return 0;
}

}


