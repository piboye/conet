/*
 * =====================================================================================
 *
 *       Filename:  pb_rpbc_server.h
 *
 *    Description:  
 *
 *        Version:  1.0
 *        Created:  2014年05月25日 03时18分43秒
 *       Revision:  none
 *       Compiler:  gcc
 *
 *         Author:  piboye
 *   Organization:  
 *
 * =====================================================================================
 */
#ifndef __ASYNC_PB_RPC_SERVER_H_INC__
#define __ASYNC_PB_RPC_SERVER_H_INC__
#include <string>

#include "rpc_pb_server_base_impl.h"

#include "http_server.h"
#include "tcp_server.h"
#include "google/protobuf/message.h"
#include "cmd_base.h"

#include "../base/obj_pool.h"
#include "../base/net_tool.h"

namespace conet
{

struct tcp_server_t;
struct rpc_pb_server_base_t;

struct async_rpc_pb_server_t : public ServerBase
{
    tcp_server_t * tcp_server;
    rpc_pb_server_base_t * base_server;

    //业务处理协程池
    obj_pool_t worker_pool;
    uint64_t m_req_num;
    

    async_rpc_pb_server_t();

    int init(
        rpc_pb_server_base_t *base_server,
        tcp_server_t * tcp_server
        );

    int start();

    int stop(int wait_ms);
    ~async_rpc_pb_server_t();


    int main_proc(conn_info_t *conn);

    int proc_worker();

    coroutine_t * stat_co;

};

}


#endif /* end of include guard */ 
