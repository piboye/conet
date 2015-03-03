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
                ret = serialize_cmd_base_impl(out, olen, &cmd_base, NULL);
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
                ret = serialize_cmd_base_impl(out,olen, &cmd_base, NULL);
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

        ret = serialize_cmd_base_impl(out, olen, &cmd_base, rsp);
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
    to_stop = 1;
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




// async process
//
//
//
//

struct rpc_pb_conn_asyc_ctx_t;

struct rpc_pb_cmd_ctx_t
{
    rpc_pb_conn_asyc_ctx_t * conn_ctx;
    cmd_base_t *cmd_base;
    rpc_pb_cmd_t *cmd; 
};

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

struct rpc_pb_conn_asyc_ctx_t
{
    conn_info_t *conn;
    int fd;
    tcp_server_t * tcp_server;
    rpc_pb_server_t * server;
    int cur_req_num;

    std::vector<std::vector<char>*> tx_queue;

    uint32_t  tx_bytes;

    ObjPool<std::vector<char> >  free_buffers;

    ObjPool<rpc_pb_cmd_ctx_t>  cmd_ctx_pool;

    conet::CondWaitQueue rsp_wait;

    int to_stop;

    int w_stop;
    int r_stop;

    //工作协程 引用数
    int work_ref_num;

    //判断是否需要立刻唤醒发送者
    static int is_send_data(void *arg)
    {
        rpc_pb_conn_asyc_ctx_t * self = (rpc_pb_conn_asyc_ctx_t *)(arg);
        if (self->tx_queue.size() >= 100) {
            return 1;
        }

        if (self->tx_bytes >= 1400)
        {
            return 2;
        }

        return 0;
    }

    rpc_pb_conn_asyc_ctx_t()
    {
        tx_bytes = 0;
        this->rsp_wait.init(&rpc_pb_conn_asyc_ctx_t::is_send_data, this, 0);

        w_stop = 0;
        r_stop = 0;
        to_stop = 0;
        work_ref_num = 0;
    }

    ~rpc_pb_conn_asyc_ctx_t()
    {
        for (size_t i=0; i<tx_queue.size(); ++i)
        {
            delete tx_queue[i];
        }
    }

    int proc_write()
    {
        rpc_pb_conn_asyc_ctx_t *self = this;
        int fd = dup(self->fd);
        tcp_server_t * tcp_server= self->tcp_server; 

        std::vector<std::vector<char> *> out_datas;

        int ret = 0;
        while (0 == tcp_server->to_stop && 0 == self->to_stop)
        {
            out_datas.swap(self->tx_queue);
            if (out_datas.empty()) {
                self->rsp_wait.wait_on(1000);
                continue;
            }

            self->tx_bytes = 0;

            ret = write_all(fd, out_datas);

            if (ret <0) {
                break;
            }

            for(size_t i=0, cnt = out_datas.size(); i< cnt; ++i)
            {
                self->free_buffers.release(out_datas[i]);
            }
            out_datas.clear();
            if (ret) {
                break;
            }
        }
        self->w_stop = 1;
        close(fd);
        return 0;
    }

    int proc_read()
    {
        tcp_server_t * tcp_server = this->tcp_server;
        rpc_pb_server_t* server = this->server;
        rpc_pb_server_base_t * base_server = server->base_server;

        conn_info_t *conn = this->conn;

        int ret = 0;

        int fd = dup(conn->fd);

        PacketStream  *stream= new PacketStream(10240);
        stream->init(fd);

        while(0 == tcp_server->to_stop && 0 == this->w_stop)
        {

            struct pollfd pf = { fd: fd, events: POLLIN | POLLERR | POLLHUP};

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
            ret = stream->read_packet(&data, &packet_len, 1000, 1); 

            if (ret == 0) {
                break;
            }

            if (ret <0) {
                LOG(ERROR)<<"read 4 byte pack failed, fd:"<<fd<<", ret:"<<ret;
                break;
            }

            while (1) { 
                if ((data == NULL) || (packet_len <=0)) {
                    LOG(ERROR)<<"recv data failed, fd:"<<fd<<", ret:"<<ret;
                    break;        
                }

                cmd_base_t * cmd_base = new cmd_base_t();
                cmd_base->init();
                int ret = cmd_base->parse(data, packet_len);

                if (ret)
                {
                    // parse cmd base head failed;
                    LOG(ERROR)<<"parse cmd base failed, fd:"<<fd<<", ret:"<<ret;
                    break;
                }


                if (cmd_base->type != CmdBase::REQUEST_TYPE) {
                    LOG(ERROR)<<"request type["<<cmd_base->type<<"] error, require REQUEST_TYPE:"<<CmdBase::REQUEST_TYPE;
                    break;
                }

                rpc_pb_cmd_t * cmd = NULL; 
                uint64_t cmd_id = cmd_base->cmd_id;
                if (cmd_id > 0) {
                    cmd = base_server->get_rpc_pb_cmd(cmd_id);
                    if (NULL == cmd) {
                        // not unsuppend cmd;
                        LOG(ERROR)<< "unsuppend cmd id:"<<cmd_id;

                        cmd_base->ret = CmdBase::ERR_UNSUPPORED_CMD;

                        errmsg = "unsuppored cmd_id:";
                        append_to_str(cmd_id, &errmsg);

                        init_ref_str(&cmd_base->errmsg, errmsg);
                        //ret = server->send_pb(fd, cmd_base);
                        if (ret <= 0) {
                            LOG(ERROR)<<"send resp failed!, fd:"<<fd<<", ret:"<<ret;
                        }
                        break;
                    }

                } else if (cmd_base->cmd_name.len >0) {
                    ref_str_t cmd_name = cmd_base->cmd_name;
                    cmd = base_server->get_rpc_pb_cmd(cmd_name.data, cmd_name.len);
                    if (NULL == cmd) {
                        // not unsuppend cmd;
                        std::string cmd_name_s;
                        ref_str_to(&cmd_name, &cmd_name_s);
                        LOG(ERROR)<< "unsuppend cmd:"<<cmd_name_s;

                        cmd_base->ret = CmdBase::ERR_UNSUPPORED_CMD;
                        errmsg ="unsuppored cmd:"+cmd_name_s;
                        init_ref_str(&cmd_base->errmsg, errmsg);
                        //ret = server->send_pb(fd, cmd_base);
                        if (ret <= 0) {
                            LOG(ERROR)<<"send resp failed!, fd:"<<fd<<", ret:"<<ret;
                        }
                        break;
                    }
                }

                rpc_pb_cmd_ctx_t *cmd_ctx = this->cmd_ctx_pool.alloc();
                cmd_ctx->cmd = cmd;
                cmd_ctx->cmd_base = cmd_base;
                cmd_ctx->conn_ctx = this;

                coroutine_t * worker = (coroutine_t *)server->worker_pool.alloc();

                resume(worker, cmd_ctx);
                ret =  stream->next_packet(&data, &packet_len);
                if (ret != 0) {
                    break;
                }
            }
        }

        delete stream;
        close(fd);
        this->r_stop = 1;
        return 0;
    }
};

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
        ++cmd_ctx->conn_ctx->work_ref_num;

        rpc_ctx.conn_info = cmd_ctx->conn_ctx->conn;

        rpc_pb_cmd_t * cmd = cmd_ctx->cmd;  

        cmd_base_t *cmd_base = cmd_ctx->cmd_base;

        int retcode = 0 ;

        //rpc_ctx.req = cmd_base;

        rpc_ctx.to_close = 0;

        google::protobuf::Message *rsp = NULL;

        retcode = rpc_pb_call_cb(cmd, &rpc_ctx, cmd_base->body, &rsp, &errmsg);

        uint64_t seq_id = cmd_base->seq_id;
        cmd_base->init();
        cmd_base->type = conet_rpc_pb::CmdBase::RESPONSE_TYPE;
        cmd_base->ret = retcode;
        cmd_base->seq_id = seq_id;
        if (!errmsg.empty()) {
            init_ref_str(&cmd_base->errmsg, errmsg);
        }
        
        //TODO: errmsg 需要处理

        // 给发送队列
        std::vector<char> * out = cmd_ctx->conn_ctx->free_buffers.alloc();

        serialize_cmd_base(out, cmd_base, rsp);
        cmd_ctx->conn_ctx->tx_queue.push_back(out);
        cmd_ctx->conn_ctx->tx_bytes += out->size();

        if (rsp) {
            cmd->rsp_pool.release(rsp);
        }
        delete cmd_ctx->cmd_base;
        cmd_ctx->cmd_base = NULL;

        ++self->async_req_num;

        //唤醒发送者
        cmd_ctx->conn_ctx->rsp_wait.wakeup_all();

        // 释放
        cmd_ctx->conn_ctx->cmd_ctx_pool.release(cmd_ctx);

        --cmd_ctx->conn_ctx->work_ref_num;

        //睡眠
        if (0 == self->to_stop) break;
        self->worker_pool.release(co_self);
        cmd_ctx = (rpc_pb_cmd_ctx_t *) yield();
    } while (cmd_ctx && 0 == self->to_stop);

    return 0;
}



static 
int proc_tcp_rpc_pb_async(rpc_pb_server_t *server, conn_info_t *conn)
{

    tcp_server_t * tcp_server = (tcp_server_t *)conn->server;

    rpc_pb_conn_asyc_ctx_t a_ctx;

    a_ctx.fd = conn->fd;
    a_ctx.conn = conn;
    a_ctx.server = server;
    a_ctx.tcp_server = tcp_server;
    a_ctx.cur_req_num = 0;

    conet::coroutine_t * r_co = conet::alloc_coroutine(ptr_cast<co_main_func_t>(&rpc_pb_conn_asyc_ctx_t::proc_read), &a_ctx);
    conet::resume(r_co);

    conet::coroutine_t * w_co = conet::alloc_coroutine(ptr_cast<co_main_func_t>(&rpc_pb_conn_asyc_ctx_t::proc_write), &a_ctx);
    conet::resume(w_co);

    conet::wait(r_co);
    conet::free_coroutine(r_co);


    close(conn->fd);

    a_ctx.to_stop = 1;

    while (a_ctx.work_ref_num > 0) 
    {
        // 10ms 检查一下
        usleep(10000);
    }

    //a_ctx.rsp_wait.wakeup();
    conet::wait(w_co);
    conet::free_coroutine(w_co);

    return 0;
}


}
