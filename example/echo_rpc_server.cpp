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
#include "base/plog.h"
#include "svrkit/server_common.h"
#include "base/module.h"

using namespace conet;

static
volatile uint64_t cnt=0;
class EchoServer
{
    public:
        EchoServer() {
            m_cur_cnt = 0;
            m_prev_cnt = 0;
        }

        std::string m_pre;
        int m_cur_cnt;
        int m_prev_cnt;

        int proc_echo_impl(rpc_pb_ctx_t *ctx,
                EchoReq *req, EchoResp *resp, std::string *errmsg) 
        {

            //resp->set_msg(m_pre + req->msg()); 
            resp->set_msg(req->msg()); 
            m_cur_cnt++;
            //LOG(ERROR)<<req->GetDescriptor()->DebugString();
            //LOG(INFO)<<req->msg();
            return 0;
        }

};

EchoServer echo_server;

DEFINE_MODULE(echo_server)
{
    std::string a;
    echo_server.m_pre="fuck:";
    CO_RUN((a), {
        while(!conet::get_server_stop_flag()) {
            int cnt = echo_server.m_cur_cnt - echo_server.m_prev_cnt;
            echo_server.m_prev_cnt = echo_server.m_cur_cnt;
            if (cnt >0) {
                PLOG_INFO("qps:", cnt);
            }
            sleep(1);
        }
    });
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
