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
#include "server/rpc_pb_server.h"
#include "example/echo_rpc.pb.h"

using namespace conet;

rpc_pb_server_t g_server;


int proc_echo_impl(void *arg, rpc_pb_ctx_t *ctx,
        EchoReq *req, EchoResp *resp, std::string *errmsg) 
{
   resp->set_msg(req->msg()); 
   return 0;
}

REGISTRY_RPC_PB_FUNC(echo, echo, proc_echo_impl, NULL)


int main(int argc, char const* argv[])
{
    if (argc < 3) {
        fprintf(stderr, "usage:%s ip port\n", argv[0]);
        return 0;
    }
    char const * ip = argv[1];
    int  port = atoi(argv[2]);
    fprintf(stderr, "listen to %s:%d\n", ip, port);
    server_t base_server;
    int ret = 0;
    ret = init_server(&base_server, ip, port);
    g_server.server = &base_server;
    g_server.server_name = "echo"; 

    ret = get_global_server_cmd(&g_server);

    start_server(&g_server);
    while (conet::get_epoll_pend_task_num() >0) {
        conet::dispatch();
    }
    return 0;
}

