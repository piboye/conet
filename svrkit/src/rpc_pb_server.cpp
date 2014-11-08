/*
 * =====================================================================================
 *
 *       Filename:  rpc_pb_server.cpp
 *
 *    Description:  
 *
 *        Version:  1.0
 *        Created:  2014年05月25日 07时48分45秒
 *       Revision:  none
 *       Compiler:  gcc
 *
 *         Author:  YOUR NAME (), 
 *   Organization:  
 *
 * =====================================================================================
 */
#include <stdlib.h>
#include <queue>
#include "rpc_pb_server.h"
#include "rpc_pb_server_base.h"
#include "rpc_pb_server_base_impl.h"
#include "tcp_server.h"
#include "http_server.h"
#include "base/incl/fn_ptr_cast.h"


#include "base/incl/net_tool.h"
#include "base/incl/auto_var.h"
#include "glog/logging.h"
#include "core/incl/wait_queue.h"
#include "base/incl/obj_pool.h"

using namespace conet_rpc_pb;



namespace conet
{

static 
inline
void append_to_str(uint64_t v, std::string *out)
{
    char buf[40];
    size_t len = 0;
    len = snprintf(buf, sizeof(buf), "%lu", v);
    out->append(buf, len);
}

static int proc_rpc_pb(rpc_pb_server_t * server, conn_info_t *conn);

rpc_pb_server_t::rpc_pb_server_t()
{
    tcp_server = NULL;
    base_server = NULL;
    http_server = NULL;
}

static 
void free_packet_stream(void *arg, void *ps)
{
    PacketStream *ps2 = (PacketStream *)(ps);
    delete ps2;
}


PacketStream *rpc_pb_server_t::alloc_packet_stream()
{
    int max_size = this->tcp_server->conf.max_packet_size;    
    PacketStream *ps = new PacketStream(max_size);
    ps->is_http = this->http_server && (this->tcp_server == this->http_server->tcp_server);
    return ps;
}

int rpc_pb_server_t::init(
        rpc_pb_server_base_t *base_server,
        tcp_server_t * tcp_server,
        http_server_t * http_server
    )
{
    this->base_server = base_server;
    this->tcp_server = tcp_server;
    this->http_server = http_server;
    if (http_server)  {
        base_server->registry_all_rpc_http_api(http_server, this->http_base_path);
    }
    tcp_server->set_conn_cb(fn_ptr_cast<tcp_server_t::conn_proc_cb_t>(&proc_rpc_pb), this);
    this->m_packet_stream_pool.set_alloc_obj_func(fn_ptr_cast<obj_pool_t::alloc_func_t>(&rpc_pb_server_t::alloc_packet_stream), this);
    this->m_packet_stream_pool.set_free_obj_func(free_packet_stream, NULL);

    return 0;
}

int rpc_pb_server_t::start()
{
    int ret =  0;
    ret = tcp_server->start();
    return ret;
}

int rpc_pb_server_t::send_pb(int fd, google::protobuf::Message const &msg)
{
    PacketStream *ps = (PacketStream *)this->m_packet_stream_pool.alloc();
    ps->init(fd);
    int ret =  send_pb_obj(fd, msg, ps->buff, ps->max_size, 1000 /* 1s */);
    this->m_packet_stream_pool.release(ps);
    return ret;
}

static int proc_rpc_pb(rpc_pb_server_t * server, conn_info_t *conn)
{
    tcp_server_t * tcp_server = (tcp_server_t *)conn->server;
    rpc_pb_server_base_t * base_server = server->base_server;
    http_server_t *http_server = server->http_server;

    int ret = 0;

    int fd = conn->fd;

    rpc_pb_ctx_t ctx;

    CmdBase cmd_base;

    ctx.server = server;
    ctx.conn_info = conn;
    ctx.req = &cmd_base;
    ctx.to_close = 0;

    while(0 == tcp_server->to_stop)
    {

        struct pollfd pf = {
            fd: fd,
            events: POLLIN | POLLERR | POLLHUP
        };

        ret = poll( &pf, 1, 10000);
        if (ret == 0) {
            //timeout
            continue;
        }

        if (ret <0) {
            /* 
            if (errno == EINTR) {
                continue;
            }
            */
            break;
        }

        if ((pf.revents & POLLERR) || (pf.revents &POLLHUP)) {
            break;
        }

        char * data = NULL;
        int packet_len = 0;

        PacketStream *stream = (PacketStream *) server->m_packet_stream_pool.alloc();
        stream->init(fd);
        ret = stream->read_packet(&data, &packet_len, 100, 1); 
        if (ret == 0) {
            server->m_packet_stream_pool.release(stream);
            break;
        }

        if (ret <0) {
            if (ret == PacketStream::HTTP_PROTOCOL_DATA) {
                    conn->extend = stream;
                    http_server->conn_proc(conn);
            } else {
                LOG(ERROR)<<"read 4 byte pack failed, fd:"<<fd<<", ret:"<<ret;
            }
            server->m_packet_stream_pool.release(stream);
            break;
        }

        server->m_packet_stream_pool.release(stream);

        if ((data == NULL) || (packet_len <=0)) {
            LOG(ERROR)<<"recv data failed, fd:"<<fd<<", ret:"<<ret;
            break;        
        }

        if (!cmd_base.ParseFromArray(data, packet_len)) 
        {
            // parse cmd base head failed;
            LOG(ERROR)<<"parse cmd base failed, fd:"<<fd<<", ret:"<<ret;
            break;
        }
        

        if (cmd_base.type() != CmdBase::REQUEST_TYPE) {
            LOG(ERROR)<<"request type["<<cmd_base.type()<<"] error, require REQUEST_TYPE:"<<CmdBase::REQUEST_TYPE;
            break;
        }

        rpc_pb_cmd_t * cmd = NULL; 
        uint64_t cmd_id = cmd_base.cmd_id();
        if (cmd_id > 0) {
            cmd = base_server->get_rpc_pb_cmd(cmd_id);
            if (NULL == cmd) {
                // not unsuppend cmd;
                LOG(ERROR)<< "unsuppend cmd id:"<<cmd_id;

                cmd_base.set_ret(CmdBase::ERR_UNSUPPORED_CMD);

                std::string errmsg = "unsuppored cmd_id:";
                append_to_str(cmd_id, &errmsg);

                cmd_base.set_errmsg(errmsg);
                ret = server->send_pb(fd, cmd_base);
                if (ret <= 0) {
                    LOG(ERROR)<<"send resp failed!, fd:"<<fd<<", ret:"<<ret;
                }
                break;
            }
            
        } else {
            std::string const & cmd_name = cmd_base.cmd_name();
            cmd = base_server->get_rpc_pb_cmd(cmd_name.c_str(), cmd_name.size());
            if (NULL == cmd) {
                // not unsuppend cmd;
                LOG(ERROR)<< "unsuppend cmd:"<<cmd_name;

                cmd_base.set_ret(CmdBase::ERR_UNSUPPORED_CMD);
                cmd_base.set_errmsg("unsuppored cmd:"+cmd_name);
                ret = server->send_pb(fd, cmd_base);
                if (ret <= 0) {
                    LOG(ERROR)<<"send resp failed!, fd:"<<fd<<", ret:"<<ret;
                }
                break;
            }
        }

        std::string * req = cmd_base.mutable_body();


        int retcode = 0;
        std::string *rsp = cmd_base.mutable_body();
        std::string *errmsg = cmd_base.mutable_errmsg();
        retcode = rpc_pb_call_cb(cmd, &ctx, req, rsp, errmsg);

        cmd_base.set_type(CmdBase::RESPONSE_TYPE);
        cmd_base.set_ret(retcode);

        ret = server->send_pb(fd, cmd_base);
        if (ret <=0) {
            // send data failed;
            LOG(ERROR)<<"send resp failed!, fd:"<<fd<<", ret:"<<ret;
            break; 
        }
        if (ctx.to_close) {
            break;
        }
    } 
    return 0;
}




int rpc_pb_server_t::stop(int wait)
{
    int ret = 0;
    LOG(INFO)<<"stop rpc main server";
    ret = tcp_server->stop(wait);
    if (ret) {
        LOG(INFO)<<"stop rpc main server success";
    } else {
        LOG(INFO)<<"stop rpc main server failed, [ret:"<<ret<<"]";
    }
    
    LOG(INFO)<<"stop rpc http server";
    if (http_server) {
        ret = http_server->stop(wait);
        if (ret) {
            LOG(ERROR)<<"stop rpc http server failed, [ret:"<<ret<<"]";
        } else {
            LOG(INFO)<<"stop rpc http server success";
        }
    }

    LOG(INFO)<<"stop rpc finished";
    return ret;
}
}
