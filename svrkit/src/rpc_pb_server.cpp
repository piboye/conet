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
 *         Author:  piboye
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
#include "base/ptr_cast.h"


#include "base/net_tool.h"
#include "base/auto_var.h"
#include "core/wait_queue.h"
#include "base/obj_pool.h"

#include "cmd_base.h"

#include "glog/logging.h"

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

static int proc_tcp_rpc_pb(rpc_pb_server_t * server, conn_info_t *conn);
static int proc_udp_rpc_pb(rpc_pb_server_t * server, conn_info_t *conn, char const *data, size_t len, char *out, size_t *olen)
{
    rpc_pb_server_base_t * base_server = server->base_server;
    int ret = 0;
    
    rpc_pb_ctx_t ctx;

    ctx.server = server;
    ctx.conn_info = conn;
    ctx.to_close = 0;
    cmd_base_t & cmd_base = ctx.cmd_base;
    cmd_base.init();
    ret = ctx.cmd_base.parse(data, len);
    std::string errmsg;
    int fd = conn->fd;
    do {
        if (ret)
        {
            // parse cmd base head failed;
            LOG(ERROR)<<"parse cmd base failed, fd:"<<conn->fd<<", ret:"<<ret;
            *olen = 0;
            break;
        }


        if (cmd_base.type != CmdBase::REQUEST_TYPE) {
            LOG(ERROR)<<"request type["<<cmd_base.type<<"] error, require REQUEST_TYPE:"<<CmdBase::REQUEST_TYPE;
            *olen = 0;
            break;
        }

        rpc_pb_cmd_t * cmd = NULL; 
        uint64_t cmd_id = cmd_base.cmd_id;
        if (cmd_id > 0) {
            cmd = base_server->get_rpc_pb_cmd(cmd_id);
            if (NULL == cmd) {
                // not unsuppend cmd;
                LOG(ERROR)<< "unsuppend cmd id:"<<cmd_id;

                cmd_base.ret = CmdBase::ERR_UNSUPPORED_CMD;

                errmsg = "unsuppored cmd_id:";
                append_to_str(cmd_id, &errmsg);

                init_ref_str(&cmd_base.errmsg, errmsg);
                ret = serialize_cmd_base(out, olen, &cmd_base, NULL);
                if (ret < 0) {
                    LOG(ERROR)<<"send resp failed!, fd:"<<fd<<", ret:"<<ret;
                }
                break;
            }
        } else if (cmd_base.cmd_name.len >0) {
            ref_str_t cmd_name = cmd_base.cmd_name;
            cmd = base_server->get_rpc_pb_cmd(cmd_name.data, cmd_name.len);
            if (NULL == cmd) {
                // not unsuppend cmd;
                std::string cmd_name_s;
                ref_str_to(&cmd_name, &cmd_name_s);
                LOG(ERROR)<< "unsuppend cmd:"<<cmd_name_s;

                cmd_base.ret = CmdBase::ERR_UNSUPPORED_CMD;
                errmsg ="unsuppored cmd:"+cmd_name_s;
                init_ref_str(&cmd_base.errmsg, errmsg);
                ret = serialize_cmd_base(out,olen, &cmd_base, NULL);
                if (ret < 0) {
                    LOG(ERROR)<<"send resp failed!, fd:"<<fd<<", ret:"<<ret;
                }
                break;
            }
        }



        int retcode = 0;
        google::protobuf::Message *rsp = NULL;
        retcode = rpc_pb_call_cb(cmd, &ctx, cmd_base.body, &rsp, &errmsg);

        cmd_base.init();
        cmd_base.type = CmdBase::RESPONSE_TYPE;
        cmd_base.ret = retcode;

        ret = serialize_cmd_base(out, olen, &cmd_base, rsp);
        if (rsp) {
            cmd->rsp_pool.release(rsp);
        }
        if (ret <=0) {
            // send data failed;
            LOG(ERROR)<<"send resp failed!, fd:"<<fd<<", ret:"<<ret;
            break;
        } 
    } while(0);
    return 0;
}

rpc_pb_server_t::rpc_pb_server_t()
{
    base_server = NULL;
}

static 
void free_packet_stream(void *arg, void *ps)
{
    PacketStream *ps2 = (PacketStream *)(ps);
    delete ps2;
}

PacketStream *rpc_pb_server_t::alloc_packet_stream()
{
    int max_size = this->max_packet_size;
    PacketStream *ps = new PacketStream(max_size);
    ps->is_http =  0;
    return ps;
}

int rpc_pb_server_t::init(
        rpc_pb_server_base_t *base_server)
{
    this->max_packet_size = 10240;
    this->base_server = base_server;
    this->m_packet_stream_pool.set_alloc_obj_func(ptr_cast<obj_pool_t::alloc_func_t>(&rpc_pb_server_t::alloc_packet_stream), this);
    this->m_packet_stream_pool.set_free_obj_func(free_packet_stream, NULL);

    return 0;
}

int rpc_pb_server_t::add_server(tcp_server_t *server)
{
    server->set_conn_cb(ptr_cast<tcp_server_t::conn_proc_cb_t>(&proc_tcp_rpc_pb), this);
    this->m_servers.push_back(server);
    return 0;
}

int rpc_pb_server_t::add_server(udp_server_t *server)
{
    server->set_conn_cb(ptr_cast<udp_server_t::conn_proc_cb_t>(&proc_udp_rpc_pb), this);
    this->m_servers.push_back(server);
    return 0;
}

int rpc_pb_server_t::add_server(http_server_t *server)
{
    base_server->registry_all_rpc_http_api(server, this->http_base_path);
    this->m_servers.push_back(server);
    return 0;
}

int rpc_pb_server_t::start()
{
    int ret =  0;
    for (size_t i= 0; i< m_servers.size(); ++i)
    {
        ret = m_servers[i]->start();
    }
    return ret;
}

int rpc_pb_server_t::stop(int wait)
{
    int ret = 0;
    for (size_t i= 0; i< m_servers.size(); ++i)
    {
        ret = m_servers[i]->stop(wait);
    }
    
    LOG(INFO)<<"stop rpc finished";
    return ret;
}


int rpc_pb_server_t::send_pb(int fd, cmd_base_t *cmd_base, google::protobuf::Message *rsp)
{
    int ret = 0;
    PacketStream *ps  = (PacketStream *)this->m_packet_stream_pool.alloc();
    ret = send_cmd_base(fd, ps, cmd_base, rsp, 1000);
    this->m_packet_stream_pool.release(ps);
    return ret;
}



static int proc_tcp_rpc_pb(rpc_pb_server_t * server, conn_info_t *conn)
{
    tcp_server_t * tcp_server = (tcp_server_t *)conn->server;
    rpc_pb_server_base_t * base_server = server->base_server;
    int ret = 0;

    int fd = conn->fd;

    rpc_pb_ctx_t ctx;

    ctx.server = server;
    ctx.conn_info = conn;
    ctx.to_close = 0;
    cmd_base_t & cmd_base = ctx.cmd_base;

    while(0 == tcp_server->to_stop)
    {

        struct pollfd pf = {
            fd: fd,
            events: POLLIN | POLLERR | POLLHUP
        };

        ret = co_poll( &pf, 1, 10000);
        if (ret == 0) {
            //timeout
            continue;
        }

        if (ret <0) {
            if (errno == EINTR) {
                continue;
            }
            break;
        }

        if ((pf.revents & POLLERR) || (pf.revents &POLLHUP)) {
            break;
        }

        std::string errmsg;
        char * data = NULL;
        int packet_len = 0;

        PacketStream *stream = (PacketStream *) server->m_packet_stream_pool.alloc();
        stream->init(fd);
        ret = stream->read_packet(&data, &packet_len, 100, 1); 
        if (ret == 0) {
            server->m_packet_stream_pool.release(stream);
            break;
        }

        if (ret <0) 
        {
            if (ret == PacketStream::HTTP_PROTOCOL_DATA) {
                conn->extend = stream;
                //http_server->conn_proc(conn);
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

        cmd_base.init();
        int ret = ctx.cmd_base.parse(data, packet_len);

        if (ret)
        {
            // parse cmd base head failed;
            LOG(ERROR)<<"parse cmd base failed, fd:"<<fd<<", ret:"<<ret;
            break;
        }
        

        if (cmd_base.type != CmdBase::REQUEST_TYPE) {
            LOG(ERROR)<<"request type["<<cmd_base.type<<"] error, require REQUEST_TYPE:"<<CmdBase::REQUEST_TYPE;
            break;
        }

        rpc_pb_cmd_t * cmd = NULL; 
        uint64_t cmd_id = cmd_base.cmd_id;
        if (cmd_id > 0) {
            cmd = base_server->get_rpc_pb_cmd(cmd_id);
            if (NULL == cmd) {
                // not unsuppend cmd;
                LOG(ERROR)<< "unsuppend cmd id:"<<cmd_id;

                cmd_base.ret = CmdBase::ERR_UNSUPPORED_CMD;

                errmsg = "unsuppored cmd_id:";
                append_to_str(cmd_id, &errmsg);

                init_ref_str(&cmd_base.errmsg, errmsg);
                ret = server->send_pb(fd, &cmd_base);
                if (ret <= 0) {
                    LOG(ERROR)<<"send resp failed!, fd:"<<fd<<", ret:"<<ret;
                }
                break;
            }
            
        } else if (cmd_base.cmd_name.len >0) {
            ref_str_t cmd_name = cmd_base.cmd_name;
            cmd = base_server->get_rpc_pb_cmd(cmd_name.data, cmd_name.len);
            if (NULL == cmd) {
                // not unsuppend cmd;
                std::string cmd_name_s;
                ref_str_to(&cmd_name, &cmd_name_s);
                LOG(ERROR)<< "unsuppend cmd:"<<cmd_name_s;

                cmd_base.ret = CmdBase::ERR_UNSUPPORED_CMD;
                errmsg ="unsuppored cmd:"+cmd_name_s;
                init_ref_str(&cmd_base.errmsg, errmsg);
                ret = server->send_pb(fd, &cmd_base);
                if (ret <= 0) {
                    LOG(ERROR)<<"send resp failed!, fd:"<<fd<<", ret:"<<ret;
                }
                break;
            }
        }



        int retcode = 0;
        google::protobuf::Message *rsp = NULL;
        retcode = rpc_pb_call_cb(cmd, &ctx, cmd_base.body, &rsp, &errmsg);

        cmd_base.init();
        cmd_base.type = CmdBase::RESPONSE_TYPE;
        cmd_base.ret = retcode;

        ret = server->send_pb(fd, &cmd_base, rsp);
        if (rsp) {
            cmd->rsp_pool.release(rsp);
        }
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





rpc_pb_server_t::~rpc_pb_server_t()
{

}
}
