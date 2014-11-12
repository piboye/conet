/*
 * =====================================================================================
 *
 *       Filename:  rpc_pb_client_duplex.cpp
 *
 *    Description:  
 *
 *        Version:  1.0
 *        Created:  2014年10月23日 15时45分56秒
 *       Revision:  none
 *       Compiler:  gcc
 *
 *         Author:  YOUR NAME (), 
 *   Organization:  
 *
 * =====================================================================================
 */
#include <stdlib.h>
#include "rpc_pb_client_duplex.h"
#include "thirdparty/glog/logging.h"
#include "cmd_base.h"

namespace conet
{

    class RpcPbClientDuplex::ReqCtx
    {
    public:
        char const *m_cmd_name;

        google::protobuf::Message const * m_req;
        google::protobuf::Message * m_rsp; 
        int * m_retcode;
        std::string *m_errmsg;
        int m_timeout;
        uint64_t m_seq_id;

        IntMap::Node m_map_node;
        list_head m_link;
        timeout_handle_t m_th;

        conet::coroutine_t *m_co;
    };

    class RpcPbClientDuplex::TcpChannel
    {
        public:
            RpcPbClientDuplex * m_client;
            int m_fd;
            int m_rfd;
            int m_wfd;
            std::string m_ip;
            int m_port;
            int m_stop_flag;
            int m_read_stop;
            int m_send_stop;

            coroutine_t * m_mgr_co;
            coroutine_t * m_send_co;
            coroutine_t * m_read_co;

            TcpChannel(char const *ip, int port, RpcPbClientDuplex *client)
            {
                m_client = client;
                m_ip = ip;
                m_port = port;
                m_stop_flag = 0;
                m_read_stop = 0;
                m_send_stop = 0;

                m_mgr_co  = NULL;
                m_send_co = NULL;
                m_read_co = NULL;
            }

            int start() 
            {
                CO_MAIN_FUN * m_f = NULL;
                int (TcpChannel::*m_f2)(void)  = &TcpChannel::mgr_proc;
                memcpy(&m_f, &m_f2, sizeof(void *));
                m_mgr_co = alloc_coroutine(m_f, this);
                resume(m_mgr_co);
                return 0;
            }

            int stop()
            {
                m_stop_flag = 1;
                if (m_mgr_co) {
                    m_client->m_req_wait.wakeup_all();
                    conet::wait(m_mgr_co);
                    conet::free_coroutine(m_mgr_co);
                }
                return 0;
            }

            int mgr_proc()
            {
                
                while(!m_stop_flag) 
                {
                    m_fd = conet::connect_to(m_ip.c_str(), m_port);
                    if (m_fd<0) {
                        //usleep(1000);
                        continue;
                    }
                    CO_MAIN_FUN * r_f = NULL;
                    int (TcpChannel::*r_f2)(void)  = &TcpChannel::read_proc;
                    memcpy(&(r_f), &r_f2, sizeof(void *));

                    CO_MAIN_FUN * s_f = NULL;
                    int (TcpChannel::*s_f2)(void)  = &TcpChannel::send_proc;
                    memcpy(&(s_f), &s_f2, sizeof(void *));

                    m_read_co = alloc_coroutine(r_f, this);
                    m_send_co = alloc_coroutine(s_f, this);

                    resume(m_read_co);

                    resume(m_send_co);

                    conet::wait(m_read_co);
                    conet::wait(m_send_co);

                    conet::free_coroutine(m_read_co);
                    conet::free_coroutine(m_send_co);

                    close(m_fd);

                    m_fd = -1;
                }

                return 0;
            }

            int send_proc()
            {
                int ret = 0;
                std::vector<std::vector<char> *> send_datas;
                cmd_base_t cmd_base;
                cmd_base.init();
                cmd_base.type = conet_rpc_pb::CmdBase::REQUEST_TYPE;

                m_wfd = dup(m_fd);

                int fd = m_wfd;
                while((!m_stop_flag) && (!m_read_stop))
                {
                    if (list_empty(&m_client->m_req_queue)) {
                        m_client->m_req_wait.wait_on();
                        continue;
                    }

                    
                    ReqCtx *req_ctx = NULL, *n=NULL;
                    list_for_each_entry_safe(req_ctx, n, &m_client->m_req_queue, m_link) 
                    {
                        list_del_init(&req_ctx->m_link);
                        std::vector<char> *send_data = new std::vector<char>();

                        init_ref_str(&cmd_base.cmd_name, req_ctx->m_cmd_name);
                        cmd_base.seq_id = req_ctx->m_seq_id;
                        serialize_cmd_base(send_data, &cmd_base, req_ctx->m_req);
                                
                        send_datas.push_back(send_data);
                    }
                    m_client->m_req_num = 0;

                    ret = write_all(fd, send_datas);

                    for (size_t i=0; i< send_datas.size(); ++i)
                    {
                        delete send_datas[i];
                    }
                    send_datas.clear();

                    if (ret) {
                        m_send_stop = 1;
                        break;
                    }
                }
                close(m_wfd);
                return 0;
            }

            int read_rsp(PacketStream & stream)
            {
                int ret = 0;
                char * data = NULL;
                int packet_len = 0;
                conet_rpc_pb::CmdBase resp_base;

                ret = stream.read_packet(&data, &packet_len, 10000, 1);

                if (ret <=0) {
                    return 0;
                }

                while (1)
                {
                    if (data == NULL || packet_len <=0) {
                        LOG(ERROR)<<"[rpc_pb_client] nodata";
                        return 0;
                    }

                    if (!resp_base.ParseFromArray(data, packet_len)) {
                        LOG(ERROR)<<"[rpc_pb_client] parse respose failed";
                        return -6;
                    }
                    uint64_t seq_id = resp_base.seq_id();

                    conet::IntMap::Node * node = m_client->m_in_queue.find(seq_id);
                    if (node == NULL) {
                        LOG(ERROR)<<"rpc_pb_client get rsp node failed, seq_id:"
                            <<seq_id<<" rsp:"<<resp_base.ShortDebugString();
                        return -7;
                    }

                    ReqCtx *req_ctx = container_of(node, ReqCtx, m_map_node);

                    int retcode = resp_base.ret();
                    if (retcode) {
                        if (resp_base.has_errmsg()) {
                            if (req_ctx->m_errmsg) {
                                *req_ctx->m_errmsg = resp_base.errmsg();
                            }
                        }
                    }

                    if (req_ctx->m_rsp) {
                        req_ctx->m_rsp->ParseFromString(resp_base.body());
                    }

                    conet::resume(req_ctx->m_co, 0);
                    ret = stream.next_packet(&data, &packet_len);
                    if ( ret != 0 ) {
                        break;
                    }
                }
                return 0;

            }

            int read_proc()
            {
                PacketStream stream(1024*1024);
                m_rfd = dup(m_fd);
                int fd = m_rfd;
                stream.init(fd);

                int ret = 0;
                while((!m_stop_flag) && (!m_send_stop))
                {
                    ret = read_rsp(stream);
                    if (ret) {
                        m_read_stop = 1;
                        break;
                    }
                }
                close(m_rfd);
                return 0;
            }
    };
    uint64_t RpcPbClientDuplex::get_seq_id()
    {
        return ++m_seq_id;
    }

    RpcPbClientDuplex::~RpcPbClientDuplex()
    {
        stop();
    }

    RpcPbClientDuplex::RpcPbClientDuplex()
    {
        INIT_LIST_HEAD(&m_req_queue);
        m_in_queue.init(1000);
        m_seq_id = conet::rdtscp(); 
        m_req_wait.cond_func = &RpcPbClientDuplex::is_send_data;
        m_req_wait.func_arg = this;
        m_req_wait.delay_ms = 1;
        m_req_num = 0;

    }

    int RpcPbClientDuplex::is_send_data(void *arg)
    {
        RpcPbClientDuplex * self = (RpcPbClientDuplex *)(arg);
        if (self->m_req_num >= 10) {
            return 1;
        }

        return 0;
    }


    int RpcPbClientDuplex::stop() 
    {
        for (size_t i=0, len = m_channels.size(); i<len; ++i)
        {
            m_channels[i]->stop();
            delete m_channels[i];
        }
        m_channels.clear();
        return 0;
    }

    int RpcPbClientDuplex::init(char const * host_txt)
    {
        
        std::vector<ip_port_t> hosts;
        parse_ip_list(host_txt, &hosts); 
        if (hosts.empty()) {
            return -1;
        }
        int ret = 0;

        for (size_t i=0, len = hosts.size(); i<len; ++i)
        {
            TcpChannel * ch = new TcpChannel(hosts[i].ip.c_str(), hosts[i].port, this); 
            ret = ch->start();
            if (ret) {
                delete ch;
            }
            m_channels.push_back(ch);
        }
        return 0;
    }

    static
    void proc_req_timeout(void *a)
    {
        RpcPbClientDuplex::ReqCtx *ctx = (RpcPbClientDuplex::ReqCtx *)(a);
        conet::resume(ctx->m_co, (void *)(1));
    }

    int RpcPbClientDuplex::rpc_call(
            char const *cmd_name,
            google::protobuf::Message const * req, 
            google::protobuf::Message * rsp, 
            int *retcode, std::string *errmsg, int timeout)
    {
        ReqCtx req_ctx;

        req_ctx.m_cmd_name = cmd_name;

        req_ctx.m_req = req;
        req_ctx.m_rsp = rsp;
        req_ctx.m_errmsg = errmsg;
        req_ctx.m_retcode = retcode;
        req_ctx.m_timeout = timeout;
        INIT_LIST_HEAD(&req_ctx.m_link);

        uint64_t seq_id = get_seq_id();
        req_ctx.m_seq_id = seq_id;
        req_ctx.m_map_node.init(seq_id);
        m_in_queue.add(&req_ctx.m_map_node);
        
        list_add_tail(&req_ctx.m_link, &this->m_req_queue);
        init_timeout_handle(&req_ctx.m_th, (void (*)(void *))&proc_req_timeout, &req_ctx, timeout);
        set_timeout(&req_ctx.m_th, timeout);

        req_ctx.m_co = conet::current_coroutine();
        ++this->m_req_num;

        m_req_wait.wakeup_all();

        int ret = (int)(uint64_t)conet::yield(NULL, NULL);

        cancel_timeout(&req_ctx.m_th);
        list_del(&req_ctx.m_link); 
        m_in_queue.remove(&req_ctx.m_map_node); 

        if (ret == 1) {
            //timeout
           return -1;
        }


        return 0;
    }

}

