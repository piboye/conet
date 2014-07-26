/*
 * =====================================================================================
 *
 *       Filename:  echo_rpc_server.cpp
 *
 *    Description
 *
 *        Version:  1.0
 *        Created:  2014年05月25日 10时08分26秒
 *       Revision:  none
 *       Compiler:  gcc
 *
 *         Author:  piboye
 *   Organization:  
 *
 * =====================================================================================
 */
#include <stdlib.h>
#include <stdio.h>
#include <errno.h>
#include <string.h>
#include <unistd.h>
#include "svrkit/rpc_pb_server.h"
#include "example/echo_rpc.pb.h"
#include "thirdparty/glog/logging.h"
#include "thirdparty/gflags/gflags.h"
#include "svrkit/ip_list.h"

DEFINE_string(server_addr, "127.0.0.1:12314", "server address");

using namespace conet;

rpc_pb_server_t g_server;


int proc_echo_impl(void *arg, rpc_pb_ctx_t *ctx,
        EchoReq *req, EchoResp *resp, std::string *errmsg) 
{
   resp->set_msg(req->msg()); 
   LOG(INFO)<<req->msg();
   return 0;
}

REGISTRY_RPC_PB_FUNC(echo, echo, proc_echo_impl, NULL)


int main(int argc, char * argv[])
{
    google::ParseCommandLineFlags(&argc, &argv, false); 
    google::InitGoogleLogging(argv[0]);

    server_t base_server;
    int ret = 0;
    std::vector<ip_port_t> ip_list;
    parse_ip_list(FLAGS_server_addr, &ip_list);
    if (ip_list.empty()) {
        fprintf(stderr, "server_addr:%s, format error!", FLAGS_server_addr.c_str());
        return 1;
    }
    ret = init_server(&base_server, ip_list[0].ip.c_str(), ip_list[0].port);
    if (ret) {
        fprintf(stderr, "listen to %s\n, failed, ret:%d\n", FLAGS_server_addr.c_str(), ret);
        return 1;
    }
    g_server.server = &base_server;
    g_server.server_name = "echo"; 

    ret = get_global_server_cmd(&g_server);

    start_server(&g_server);
    while (conet::get_epoll_pend_task_num() >0) {
        conet::dispatch();
    }
    return 0;
}

