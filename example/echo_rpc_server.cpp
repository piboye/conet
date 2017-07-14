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
#include "thirdparty/gflags/gflags.h"
#include "base/delay_init.h"
#include "base/plog.h"

using namespace conet;

static
volatile uint64_t cnt=0;
class EchoServer
{
    public:

        std::string m_pre;

        int proc_echo_impl(rpc_pb_ctx_t *ctx,
                EchoReq *req, EchoResp *resp, std::string *errmsg) 
        {

            resp->set_msg(m_pre + req->msg()); 
            //LOG(ERROR)<<req->GetDescriptor()->DebugString();
            //LOG(INFO)<<req->msg();
            return 0;
        }

};

EchoServer echo_server;

DELAY_INIT()
{
    std::string a;
    echo_server.m_pre="fuck:";
    return 0;
}

REGISTRY_RPC_PB_FUNC(1, "echo", &EchoServer::proc_echo_impl, &echo_server);

int proc_echo_impl(void *arg, rpc_pb_ctx_t *ctx,
        EchoReq *req, EchoResp *resp, std::string *errmsg) 
{
    resp->set_msg(req->msg()); 
    PLOG_INFO(req->msg());
    return 0;
}

REGISTRY_RPC_PB_FUNC(2, "echo2", &proc_echo_impl, NULL);

int proc_test_impl(void *arg, rpc_pb_ctx_t *ctx,
        void *req, void *resp, std::string *errmsg) 
{
    return 0;
}

REGISTRY_RPC_PB_FUNC(3, "test", &proc_test_impl, NULL);
