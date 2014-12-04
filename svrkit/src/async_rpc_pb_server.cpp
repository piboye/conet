/*
 * =====================================================================================
 *
 *       Filename:  async_rpc_pb_server.cpp
 *
 *    Description:  
 *
 *        Version:  1.0
 *        Created:  2014年11月02日 20时50分56秒
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
#include <vector>
#include <list>

#include "async_rpc_pb_server.h"

#include "rpc_pb_server_base.h"
#include "rpc_pb_server_base_impl.h"
#include "tcp_server.h"
#include "http_server.h"
#include "base/incl/ptr_cast.h"


#include "base/incl/net_tool.h"
#include "base/incl/auto_var.h"
#include "glog/logging.h"
#include "core/incl/wait_queue.h"
#include "base/incl/obj_pool.h"
#include "cmd_base.h"

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

struct rpc_pb_conn_asyc_ctx_t;

struct rpc_pb_cmd_ctx_t
{
    rpc_pb_conn_asyc_ctx_t * conn_ctx;
    cmd_base_t *cmd_base;
    rpc_pb_cmd_t *cmd; 
};

static 
void free_server_work_co(void *arg, void * val)
{
    conet::coroutine_t * co = (conet::coroutine_t *) (val);
    resume(co, NULL);
}


static 
void * alloc_server_work_co(void *arg)
{
    async_rpc_pb_server_t * server  = (async_rpc_pb_server_t *)(arg);
    conet::coroutine_t * co = alloc_coroutine(ptr_cast<co_main_func_t>(&async_rpc_pb_server_t::proc_worker), server);
    set_auto_delete(co);
    conet::resume(co);
    return co;
}

int async_rpc_pb_server_t::init(
        rpc_pb_server_base_t *base_server,
        tcp_server_t * tcp_server
    )
{
    this->base_server = base_server;
    this->tcp_server = tcp_server;

    tcp_server->set_conn_cb(ptr_cast<tcp_server_t::conn_proc_cb_t>
            (&async_rpc_pb_server_t::main_proc), this);

    this->worker_pool.set_alloc_obj_func(alloc_server_work_co, this);
    this->worker_pool.set_free_obj_func(free_server_work_co, this);

    return 0;
}

struct rpc_pb_conn_asyc_ctx_t
{
    conn_info_t *conn;
    int fd;
    tcp_server_t * tcp_server;
    async_rpc_pb_server_t * server;
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
        async_rpc_pb_server_t* server = this->server;
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

int async_rpc_pb_server_t::proc_worker()
{
    tcp_server_t *tcp_server = this->tcp_server;

    rpc_pb_ctx_t rpc_ctx;

    rpc_ctx.server = this;

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

        ++this->m_req_num;

        //唤醒发送者
        cmd_ctx->conn_ctx->rsp_wait.wakeup_all();

        // 释放
        cmd_ctx->conn_ctx->cmd_ctx_pool.release(cmd_ctx);

        --cmd_ctx->conn_ctx->work_ref_num;

        //睡眠
        if (0 == tcp_server->to_stop) break;
        this->worker_pool.release(co_self);
        cmd_ctx = (rpc_pb_cmd_ctx_t *) yield();
    } while (cmd_ctx && 0 == tcp_server->to_stop);

    return 0;
}

async_rpc_pb_server_t::~async_rpc_pb_server_t()
{

}

async_rpc_pb_server_t::async_rpc_pb_server_t()
{
    tcp_server = NULL;
    base_server = NULL;
    stat_co = NULL;
    m_req_num = 0;
}

int async_rpc_pb_server_t::main_proc(conn_info_t *conn)
{

    rpc_pb_conn_asyc_ctx_t a_ctx;

    a_ctx.fd = conn->fd;
    a_ctx.conn = conn;
    a_ctx.server = this;
    a_ctx.tcp_server = this->tcp_server;
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

    //a_ctx.rsp_wait.wakeup
    conet::wait(w_co);
    conet::free_coroutine(w_co);

    return 0;
}

int proc_stat(void *arg)
{
    async_rpc_pb_server_t * self = (async_rpc_pb_server_t *)(arg);
    uint64_t prev_req_num = self->m_req_num;
    uint64_t proc_num =0;
    while(!self->tcp_server->to_stop) 
    {
        sleep(1);
        proc_num = self->m_req_num - prev_req_num;
        prev_req_num = self->m_req_num;
        LOG(INFO)<<"poc req num: "<<proc_num;
    }

    return 0;
}

int async_rpc_pb_server_t::start()
{
    int ret =  0;
    ret = tcp_server->start();

    this->stat_co = conet::alloc_coroutine((CO_MAIN_FUN *)&proc_stat, this, 4*4096);
    set_auto_delete(this->stat_co);
    resume(stat_co);

    return ret;
}

int async_rpc_pb_server_t::stop(int wait)
{
    int ret = 0;
    LOG(INFO)<<"stop rpc main server";
    ret = tcp_server->stop(wait);
    if (ret) {
        LOG(INFO)<<"stop rpc main server success";
    } else {
        LOG(INFO)<<"stop rpc main server failed, [ret:"<<ret<<"]";
    }



    LOG(INFO)<<"stop rpc finished";
    return ret;
}

}
