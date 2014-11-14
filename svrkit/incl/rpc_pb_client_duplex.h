/*
 * =====================================================================================
 *
 *       Filename:  rpc_pb_client_duplex.h
 *
 *    Description:  
 *
 *        Version:  1.0
 *        Created:  2014年10月23日 06时23分12秒
 *       Revision:  none
 *       Compiler:  gcc
 *
 *         Author:  YOUR NAME (), 
 *   Organization:  
 *
 * =====================================================================================
 */

#ifndef __RPC_PB_CLIENT_DUPLEX_H__
#define __RPC_PB_CLIENT_DUPLEX_H__

#include <stdlib.h>
#include <string>
#include <string.h>

#include <errno.h>
#include "base/incl/net_tool.h"
#include "base/incl/list.h"
#include "base/incl/ip_list.h"
#include "base/incl/int_map.h"

#include "core/incl/wait_queue.h"
#include "core/incl/conet_all.h"

#include "svrkit/rpc_base_pb.pb.h"


namespace conet 
{

class RpcPbClientDuplex 
{
public:
    class ReqCtx;
    class TcpChannel;

    list_head m_req_queue; 


    CondWaitQueue m_req_wait;

    IntMap m_in_queue;
    uint64_t m_seq_id;
    int m_req_num;
    std::vector<TcpChannel *> m_channels;

public:
    RpcPbClientDuplex();
    ~RpcPbClientDuplex();

    int init(char const * host_txt);
    int stop();

    uint64_t get_seq_id();

    int rpc_call(
            uint64_t cmd_id,
            google::protobuf::Message const * req, 
            google::protobuf::Message * rsp, 
            int *retcode, std::string *errmsg, int timeout);
    
    int rpc_call(
            char const *cmd_name,
            google::protobuf::Message const * req, 
            google::protobuf::Message * rsp, 
            int *retcode, std::string *errmsg, int timeout);

    int rpc_call_base(
            ReqCtx &req_ctx,
            google::protobuf::Message const * req, 
            google::protobuf::Message * rsp, 
            int *retcode, std::string *errmsg, int timeout);

    static int is_send_data(void *arg);
};

}

#endif /* end of include guard */
