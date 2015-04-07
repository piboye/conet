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
#include "channel.h"

#include "core/parallel.h"
#include "base/scoped_ptr.h"

using namespace conet_rpc_pb;

namespace conet
{

//declaration function
static void * alloc_server_work_co(void *arg);
static int proc_tcp_rpc_pb(rpc_pb_server_t * server, conn_info_t *conn);
static int proc_tcp_rpc_pb_async(rpc_pb_server_t * server, conn_info_t *conn);
static void free_server_work_co(void *arg, void * val);

static 
inline
void append_to_str(uint64_t v, std::string *out)
{
    char buf[40];
    size_t len = 0;
    len = snprintf(buf, sizeof(buf), "%lu", v);
    out->append(buf, len);
}

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
                ret = serialize_cmd_base_impl(out, olen, &cmd_base, NULL, 0);
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
                ret = serialize_cmd_base_impl(out,olen, &cmd_base, NULL, 0);
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

        ret = serialize_cmd_base_impl(out, olen, &cmd_base, rsp, rsp->ByteSize());
        if (rsp) {
            cmd->rsp_pool.release(rsp);
        }
        if (ret <0) {
            // serialize data failed;
            LOG(ERROR)<<"serialize resp failed!, fd:"<<fd<<", ret:"<<ret;
            break;
        } 
    } while(0);
    return 0;
}

rpc_pb_server_t::rpc_pb_server_t()
{
    INIT_LIST_HEAD(&channels);
    base_server = NULL;
    to_stop = 0;
    async_req_num = 0;

    this->worker_pool.set_alloc_obj_func(alloc_server_work_co, this);
    this->worker_pool.set_free_obj_func(free_server_work_co, this);
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
    if (server->conf.duplex) {
        server->set_conn_cb(ptr_cast<tcp_server_t::conn_proc_cb_t>(&proc_tcp_rpc_pb_async), this);
    } else {
        server->set_conn_cb(ptr_cast<tcp_server_t::conn_proc_cb_t>(&proc_tcp_rpc_pb), this);
    }
    this->m_servers.push_back(server);
    this->m_raw_servers.insert(server);
    return 0;
}

int rpc_pb_server_t::add_server(udp_server_t *server)
{
    server->set_conn_cb(ptr_cast<udp_server_t::conn_proc_cb_t>(&proc_udp_rpc_pb), this);
    this->m_servers.push_back(server);
    this->m_raw_servers.insert(server);
    return 0;
}

int rpc_pb_server_t::add_server(http_server_t *server)
{
    base_server->registry_all_rpc_http_api(server, this->http_base_path);
    this->m_servers.push_back(server);
    this->m_raw_servers.insert(server->tcp_server);
    return 0;
}

int rpc_pb_server_t::start()
{
    int ret =  0;
    for (auto server : m_raw_servers)
    {
        ret = server->start();
    }
    return ret;
}

int rpc_pb_server_t::do_stop(int wait_ms)
{
    int ret = 0;

    channel_t *ch = NULL, *n = NULL;
    list_for_each_entry_safe(ch, n, &channels, link_to)
    {
        list_del_init(&ch->link_to);
        ch->to_stop = 1;
    }

    BEGIN_PARALLEL {
        for (auto server : m_raw_servers)
        {
            DO_PARALLEL((server, wait_ms), {
                int ret = 0;
                ret = server->stop(wait_ms);
                if (ret)
                {
                    LOG(ERROR)<<"stop server failed! ";
                }
            });
        }
    } WAIT_ALL_PARALLEL();

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

    PacketStream *stream = NULL;

    while(0 == tcp_server->to_stop)
    {

        if (stream) {
            server->m_packet_stream_pool.release(stream);
            stream = NULL;
        }

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

        stream = (PacketStream *) server->m_packet_stream_pool.alloc();
        stream->init(fd);
        stream->is_http = 1;
        ret = stream->read_packet(&data, &packet_len, 100, 1); 
        if (ret == 0) {
            break;
        }

        if (ret <0) 
        {
            if (ret == PacketStream::HTTP_PROTOCOL_DATA) {
                if (tcp_server->extend)
                {
                    http_server_t * http_server = (http_server_t*)(tcp_server->extend);
                    conn->extend = stream;
                    ret = http_server->conn_proc(conn);
                    break;
                } 
            } else {
                LOG(ERROR)<<"read 4 byte pack failed, fd:"<<fd<<", ret:"<<ret;
            }
            ret  = -1;
            break;
        }

        if ((data == NULL) || (packet_len <=0)) {
            LOG(ERROR)<<"recv data failed, fd:"<<fd<<", ret:"<<ret;
            ret = -2;
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
    if (stream) {
        server->m_packet_stream_pool.release(stream);
        stream = NULL;
    }
    return 0;
}





rpc_pb_server_t::~rpc_pb_server_t()
{
    channel_t *ch = NULL, *n = NULL;
    list_for_each_entry_safe(ch, n, &channels, link_to)
    {
        list_del_init(&ch->link_to);
        ch->to_stop = 1;
    }
}




// async process
//
//
//
//

struct rpc_pb_conn_asyc_ctx_t;

static void free_server_work_co(void *arg, void * val)
{
    conet::coroutine_t * co = (conet::coroutine_t *) (val);
    resume(co, NULL);
}

static int async_proc_worker(rpc_pb_server_t *self);

static 
void * alloc_server_work_co(void *arg)
{
    rpc_pb_server_t * server  = (rpc_pb_server_t *)(arg);
    conet::coroutine_t * co = alloc_coroutine(ptr_cast<co_main_func_t>(&async_proc_worker), server);
    set_auto_delete(co);
    conet::resume(co);
    return co;
}



struct rpc_pb_cmd_ctx_t
{
    channel_t *ch;
    cmd_base_t *cmd_base;
    rpc_pb_cmd_t *cmd; 
};

static void free_send_data(void *arg, void *data, int len)
{
    delete (char *)data;
}

static int send_rpc_rsp(channel_t *ch, cmd_base_t *cmd_base, 
        int retcode, std::string *errmsg, 
        google::protobuf::Message *msg)
{

    uint64_t seq_id = cmd_base->seq_id;
    cmd_base->init();
    cmd_base->seq_id = seq_id;

    cmd_base->type = conet_rpc_pb::CmdBase::RESPONSE_TYPE;
    cmd_base->ret = retcode;
    if (!errmsg->empty()) {
        init_ref_str(&cmd_base->errmsg, *errmsg);
    }
    size_t len = 10240;
    char *out_data  = new char[10240];
    int ret = 0;

    ret = serialize_cmd_base(out_data, &len, cmd_base, msg);
    if (ret == 0) {
        ch->send_msg(out_data, len, &free_send_data, NULL);
    } else {
        delete out_data;
    }
    return 0;
}

static
int async_proc_worker(rpc_pb_server_t *self)
{
    rpc_pb_ctx_t rpc_ctx;

    rpc_ctx.server = self;

    std::string errmsg;
    coroutine_t *co_self = CO_SELF();
    rpc_pb_cmd_ctx_t *cmd_ctx = (rpc_pb_cmd_ctx_t *) yield();
    do 
    {
        rpc_ctx.conn_info = cmd_ctx->ch->conn;

        rpc_pb_cmd_t * cmd = cmd_ctx->cmd;  

        cmd_base_t *cmd_base = cmd_ctx->cmd_base;

        int retcode = 0 ;

        rpc_ctx.to_close = 0;

        google::protobuf::Message *rsp = NULL;

        errmsg.clear();
        retcode = rpc_pb_call_cb(cmd, &rpc_ctx, cmd_base->body, &rsp, &errmsg);

        // 给发送队列
        send_rpc_rsp(cmd_ctx->ch, cmd_base, retcode, &errmsg,  rsp);
        
        if (rsp) {
            cmd->rsp_pool.release(rsp);
        }

        delete cmd_ctx->cmd_base;
        cmd_ctx->cmd_base = NULL;

        ++self->async_req_num;

        delete cmd_ctx;
        cmd_ctx = NULL;

        //睡眠
        if (0 == self->to_stop) break;
        self->worker_pool.release(co_self);
        cmd_ctx = (rpc_pb_cmd_ctx_t *) yield();
    } while (cmd_ctx && 0 == self->to_stop);
    if (cmd_ctx) {
        delete cmd_ctx->cmd_base;
        delete cmd_ctx;
    }

    return 0;
}

class async_rpc_channel_ctx_t
{
public:
    rpc_pb_server_t * server;
    channel_t *ch;
    std::vector<char> read_data;  // 已经读的数据

    int read_data_cb(char const *data, int len)
    {
        if (read_data.empty()) {
            pack_len = 0;
        }
        read_data.insert(read_data.end(), data, data+len);
        do {
            size_t r_len = read_data.size();
            if (pack_len == 0 && r_len >=4) {
                pack_len = ntohl(*(uint32_t*)( &read_data[0]));
            }

            int ret = 0;
            if (r_len >= pack_len + 4) {
                ret = proc_data(&read_data[4], pack_len);
                read_data.erase(read_data.begin(), read_data.begin() + pack_len +4);
                pack_len = 0;
                if (ret) {
                    return -1;
                }
            } else {
                break;
            }
        }while (1);
        return 1;
    }

    int proc_data(char const * data, int len)
    {
        std::string errmsg;
        ScopedPtr<cmd_base_t>  cmd_base(new cmd_base_t());
        cmd_base->init();
        int ret = cmd_base->parse(data, len);

        if (ret)
        {
            // parse cmd base head failed;
            LOG(ERROR)<<"parse cmd base failed";
            return -1;
        }


        if (cmd_base->type != CmdBase::REQUEST_TYPE) {
            LOG(ERROR)<<"request type["<<cmd_base->type<<"] error, require REQUEST_TYPE:"<<CmdBase::REQUEST_TYPE;
            return -2;
        }

        rpc_pb_cmd_t * cmd = NULL; 
        uint64_t cmd_id = cmd_base->cmd_id;
        if (cmd_id > 0) {
            cmd = server->base_server->get_rpc_pb_cmd(cmd_id);
            if (NULL == cmd) {
                // not unsuppend cmd;
                LOG(ERROR)<< "unsuppend cmd id:"<<cmd_id;

                cmd_base->ret = CmdBase::ERR_UNSUPPORED_CMD;

                errmsg = "unsuppored cmd_id:";
                append_to_str(cmd_id, &errmsg);
                ret = send_rpc_rsp(this->ch, cmd_base.release(), -2, &errmsg, NULL);
                if (ret <= 0) {
                    LOG(ERROR)<<"send resp failed!";
                }
                return -3;
            }

        } else if (cmd_base->cmd_name.len >0) {
            ref_str_t cmd_name = cmd_base->cmd_name;
            cmd = server->base_server->get_rpc_pb_cmd(cmd_name.data, cmd_name.len);
            if (NULL == cmd) {
                // not unsuppend cmd;
                std::string cmd_name_s;
                ref_str_to(&cmd_name, &cmd_name_s);
                LOG(ERROR)<< "unsuppend cmd:"<<cmd_name_s;

                cmd_base->ret = CmdBase::ERR_UNSUPPORED_CMD;
                errmsg ="unsuppored cmd:"+cmd_name_s;
                ret = send_rpc_rsp(this->ch, cmd_base.release(), -2, &errmsg, NULL);
                if (ret <= 0) {
                    LOG(ERROR)<<"send resp failed!";
                }
                return -3;
            }
        }


        rpc_pb_cmd_ctx_t *cmd_ctx = new rpc_pb_cmd_ctx_t();
        cmd_ctx->cmd = cmd;
        cmd_ctx->cmd_base = cmd_base.release();
        cmd_ctx->ch = this->ch;

        coroutine_t * worker = (coroutine_t *)server->worker_pool.alloc();

        resume(worker, cmd_ctx);
        return 0;
    }

    async_rpc_channel_ctx_t()
    {
        pack_len = 0;
    }

    uint32_t pack_len;
};

static 
int proc_tcp_rpc_pb_async(rpc_pb_server_t *server, conn_info_t *conn)
{

    channel_t ch;
    ch.init(conn, server);

    async_rpc_channel_ctx_t ctx;
    ctx.server = server;
    ctx.ch = &ch;

    ch.set_new_data_cb(ptr_cast<channel_t::new_data_cb_t>(&async_rpc_channel_ctx_t::read_data_cb), 
            &ctx);

    list_add_tail(&ch.link_to, &server->channels);
    ch.start();
    ch.exit_notify.wait_on();
    list_del_init(&ch.link_to);
    ch.stop();
    return 0;
}

}
