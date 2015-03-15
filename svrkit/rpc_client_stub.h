/*
 * =====================================================================================
 *
 *       Filename:  rpc_client_stub.h
 *
 *    Description:  
 *
 *        Version:  1.0
 *        Created:  2014年09月05日 14时39分50秒
 *       Revision:  none
 *       Compiler:  gcc
 *
 *         Author:  piboye
 *   Organization:  
 *
 * =====================================================================================
 */

#include "google/protobuf/stubs/common.h"
#include "core/incl/coroutine.h"

namespace conet
{

struct RpcContext
{
    list_head link;
    uint64_t seq_id;

    char const * server_name; 
    char const * cmd_name; 
     
    uint32_t timeout;
    std::string *req_body;
    std::string *rsp_body;
    int errcode;
    std::string *errmsg;
    coroutine_t *co;
};

class RpcController
{
public:
    uint64_t m_seq_id; 

    RpcController() 
    {
        m_seq_id = 0;
    }

    uint64_t get_seq_id() 
    {
        return ++m_seq_id;
    }

    int init(char const *iplist)
    {
        parse_ip_list(iplist,  &m_ip_list);
        if (m_ip_list.empty()) {
            LOG(ERROR)<<"iplist is empty!";
            return -1;
        }
        for (size_t i = 0; i< m_ip_list.size(); ++i) 
        {
            RpcSyncChannel * channel = new RpcSyncChannel();
            channel->init(
                    m_ip_list[i].ip.c_str(), m_ip_list[i].port,
                    google::protobuf::Closure::NewPermanentCallback(this, &RpcController::get_req_ctx),
                    google::protobuf::Closure::NewPermanentCallback(this, &RpcController::put_rsp_ctx),
                    );
            channel->start();
            m_channels.push_back(channel);
        }
        return 0;
    }

    std::vector<RpcSyncChannel*> m_channels;

    std::vector<ip_port_t> m_ip_list;
    list_head req_queue;
    list_head rsp_queue;
};

class RpcSyncChannel
{
public:
    RpcChannel()
    {
        m_port = 0;
        m_get_req = NULL;
        m_put_req = NULL;
    }

    ~RpcSyncChannel() 
    {
        free_coroutine(m_co);
    }

    int init(const const *ip, int port, 
            ::google::protobuf::Closure * get_req,
            ::google::protobuf::Closure * put_rsp)
    {
        m_ip = ip;
        m_port = port;
        m_get_req = get_req;
        m_put_rsp = put_rsp;
        m_fd = -1;
        m_run = 0;
        return 0;
    }

    void start() 
    {
        m_co = alloc_coroutine(&co_mem_fun_helper<RpcSyncChannel>, this);
        conet::resume(m_co);
        m_run = 1;
    }

    int is_run() 
    {
        return m_run == 1;
    }

    void stop() 
    {
        m_run = 0;
    }

    int proc_req(RpcContext *ctx)
    {
        int ret = 0;

        conet_rpc_pb::CmdBase req_base, resp_base;

        req_base.set_server_name(ctx->server_name);
        req_base.set_cmd_name(ctx->cmd_name);
        req_base.set_seq_id(ctx->seq_id);
        req_base.set_type(conet_rpc_pb::CmdBase::REQUEST_TYPE);
        req_base.set_body(ctx->req);

        std::vector<char> out_buf;

        ret = send_pb_obj(m_fd, req_base, &out_buf, ctx->timeout);

        if (ret <=0) {
            LOG(ERROR)<<"[rpc_pb_client] send request failed, [ret:"
                <<ret<<"][errno:"<<errno<<"]"<<strerror(errno)<<"]";
            delete ctx;
            return -4;
        }

        PacketStream stream;
        stream.init(fd, 1024*1024);
        char * data = NULL;
        int packet_len = 0;

        ret = stream.read_packet(&data, &packet_len, timeout);

        if (ret <=0) {
            LOG(ERROR)<<"[rpc_pb_client] recv respose failed, [ret:"
                <<ret<<"][errno:"<<errno<<"][strerr:"<<strerror(errno)<<"]";
            return -5;
        }

        if (!resp_base.ParseFromArray(data, packet_len)) {
            LOG(ERROR)<<"[rpc_pb_client] parse respose failed";
            return -6;
        }

        ctx->retcode = resp_base.ret();
        if (ctx->retcode) {
                ctx->errmsg = resp_base.errmsg();
            return 0;
        }

        if (ctx->rsp_body) {
            *(ctx->rsp_body) = resp_base.body();
        }
        return 0;
    }

    int run() 
    {
        while (m_run) 
        {
            m_fd = net_helper::connect_to(m_ip.c_str(), m_port, 10);
            if (m_fd > 0) {
                while(m_run) {
                    RpcContext *ctx=NULL; 

                    m_get_req->Run(&ctx);

                    if (NULL == ctx) {
                        usleep(10000);
                        continue;
                    }
                    int ret = proc_req(ctx);
                    if (ret != 0) {
                        delete ctx;
                        break;
                    }
                    m_put_rsp->Run(ctx);
                }
                close(m_fd);
                m_fd = -1;
            }
        }
        return 0;
    }

    ::google::protobuf::Closure * m_get_req;
    ::google::protobuf::Closure * m_put_rsp;

    int m_run;
    int m_fd;
    std::string m_ip;
    std::string m_port;
    coroutine_t *m_co;
};

}

