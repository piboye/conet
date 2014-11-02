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
 *         Author:  YOUR NAME (), 
 *   Organization:  
 *
 * =====================================================================================
 */
#include <stdlib.h>

struct rpc_pb_cmd_ctx_t
{
    CmdBase *cmd_base;
    rpc_pb_cmd_t *cmd; 
};

struct rpc_pb_conn_asyc_ctx_t
{
    conet::ObjPool<CmdBase> *cmd_base_pool; 
    conn_info_t *conn;
    int fd;
    server_t * server_base;
    rpc_pb_server_t * server;
    int cur_req_num;

    std::queue<rpc_pb_cmd_ctx_t*> cmd_queue;
    conet::wait_queue_t req_wait;

    std::vector<std::string*> tx_queue;
    conet::wait_queue_t rsp_wait;
    int to_stop;

    int w_stop;
    int r_stop;
};

static int proc_rpc_pb_work_co(rpc_pb_conn_asyc_ctx_t *ctx)
{
    conet::ObjPool<CmdBase> *cmd_base_pool=ctx->cmd_base_pool; 
    conn_info_t *conn = ctx->conn;
    server_t * server_base= ctx->server_base; 
    rpc_pb_server_t * server = ctx->server;

    rpc_pb_ctx_t rpc_ctx;

    rpc_ctx.server = server;
    rpc_ctx.conn_info = conn;

    while (0 == server_base->to_stop && 0 == ctx->r_stop)
    {
        if (ctx->cmd_queue.empty()) 
        {
            conet::wait_on(&ctx->req_wait);
            continue;
        }

        do {
            rpc_pb_cmd_ctx_t *cmd_ctx = ctx->cmd_queue.front();
            ctx->cmd_queue.pop();
            rpc_pb_cmd_t * cmd = cmd_ctx->cmd;  
            
            CmdBase *cmd_base = cmd_ctx->cmd_base;

            int retcode = 0 ;
            std::string * req = cmd_base->mutable_body();
            std::string *rsp = cmd_base->mutable_body();
            std::string *errmsg = cmd_base->mutable_errmsg();

            rpc_ctx.req = cmd_base;

            rpc_ctx.to_close = 0;

            retcode = rpc_pb_call_cb(cmd, &rpc_ctx, req, rsp, errmsg);

            cmd_base->set_type(CmdBase::RESPONSE_TYPE);
            cmd_base->set_ret(retcode);
             
            uint32_t len = cmd_base->ByteSize();
        
            std::string *buf = new std::string();
            buf->resize(len+4);
            char * p = (char *)buf->data();
            *((uint32_t *)p) = htonl(len);
            cmd_base->SerializeToArray(p+4, len);


            cmd_base_pool->release(cmd_base);
            ctx->tx_queue.push_back(buf);
            wakeup_head(&ctx->rsp_wait);
            delete cmd_ctx;

        } while(!ctx->cmd_queue.empty()); 
    }
    ctx->to_stop = 1;
    wakeup_all(&ctx->rsp_wait);
    return 0;
}

static 
int write_all(int fd, std::vector<std::string *> const &out_datas)
{
        size_t total_len = 0;
        size_t cnt = out_datas.size();
        iovec *iov = new iovec[cnt];
        size_t *need_outs = new size_t[cnt];

        for(size_t i=0; i< cnt; ++i)
        {
            iov[i].iov_base = (void *)out_datas[i]->c_str();
            iov[i].iov_len = out_datas[i]->size();
            total_len += iov[i].iov_len;
            need_outs[i] = total_len;
        }

        size_t wret = 0; 
        size_t wlen = 0; 
        size_t start_pos = 0;
        do {
            size_t w_cnt = std::min<size_t>(cnt-start_pos, 10);
            wret = writev(fd, iov + start_pos, w_cnt);
            if (wret <=0) {
                break;
            }
            wlen += wret;
            if (wlen >= total_len) break;
            size_t pos = std::upper_bound(need_outs+start_pos, need_outs+cnt, wlen) - need_outs; 
            start_pos = pos;
            size_t nlen = wlen + iov[pos].iov_len;
            if (nlen > need_outs[pos])
            {
                size_t l = nlen-need_outs[pos];
                iov[pos].iov_base = (char *)iov[pos].iov_base + l; 
                iov[pos].iov_len -= l; 
            }
        } while(wlen < total_len);

        delete iov;
        delete need_outs;

        if (wlen == total_len) return 0;
        return -1;
}

static int proc_rpc_pb_write_co(rpc_pb_conn_asyc_ctx_t *ctx)
{
    int fd = ctx->fd;
    server_t * server_base= ctx->server_base; 

    std::vector<std::string*> out_datas;

    int ret = 0;
    while (0 == server_base->to_stop && 0 == ctx->to_stop)
    {
        out_datas.swap(ctx->tx_queue);
        if (out_datas.empty()) {
            wait_on(&ctx->rsp_wait);
            continue;
        }
        ret = write_all(fd, out_datas);
        if (ret) {
            break;
        }

        for(size_t i=0, cnt = out_datas.size(); i< cnt; ++i)
        {
            delete out_datas[i];
        }
        out_datas.clear();
    }
    ctx->r_stop = 1;
    return 0;
}

static int proc_rpc_pb_read_co(rpc_pb_conn_asyc_ctx_t *ctx)
{
    int fd = ctx->fd;
    conn_info_t *conn = ctx->conn;
    server_t * server_base= ctx->server_base; 
    rpc_pb_server_t * server = ctx->server;

    int max_size = server_base->conf.max_packet_size;

    PacketStream stream;
    stream.init(fd, max_size);

    conet::ObjPool<CmdBase> *cmd_base_pool=ctx->cmd_base_pool; 


    int ret = 0;
    while (0 == server_base->to_stop)
    {
        struct pollfd pf = {
            fd: fd,
            events: POLLIN | POLLERR | POLLHUP
        };

        ret = poll(&pf, 1, 1000);
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

        if (pf.revents & POLLERR || pf.revents &POLLHUP) {
            break;
        }

        char * data = NULL;
        int packet_len = 0;

        ret = stream.read_packet(&data, &packet_len, 100, 1); 
        if (ret == 0) {
            break;
        }

        if (ret <0) {
            if (ret == PacketStream::HTTP_PROTOCOL_DATA) {
                conn->extend = &stream;
                http_server_proc2(conn, server_base, server->http_server);
                break;
            } else {
                LOG(ERROR)<<"read 4 byte pack failed, fd:"<<fd<<", ret:"<<ret;
                break;
            }
        }
        if ((data == NULL) || (packet_len <=0)) {
            LOG(ERROR)<<"recv data failed, fd:"<<fd<<", ret:"<<ret;
            break;        
        }
        CmdBase *cmd_base = cmd_base_pool->alloc();
        if (!cmd_base->ParseFromArray(data, packet_len)) 
        {
            delete cmd_base;
            // parse cmd base head failed;
            LOG(ERROR)<<"parse cmd base failed, fd:"<<fd<<", ret:"<<ret;
            break;
        }
        

        if (cmd_base->type() != CmdBase::REQUEST_TYPE) {
            LOG(ERROR)<<"request type["<<cmd_base->type()<<"] error, require REQUEST_TYPE:"<<CmdBase::REQUEST_TYPE;
            delete cmd_base;
            break;
        }

        std::string const & cmd_name = cmd_base->cmd_name();
        rpc_pb_cmd_t * cmd = get_rpc_pb_cmd(server, cmd_name);
        if (NULL == cmd) {
            // not unsuppend cmd;
            LOG(ERROR)<< "unsuppend cmd:"<<cmd_name;

            cmd_base->set_ret(CmdBase::ERR_UNSUPPORED_CMD);
            cmd_base->set_errmsg("unsuppored cmd");
            /*
            ret = send_pb_obj(fd, cmd_base, &out_buf);
            if (ret <= 0) {
                LOG(ERROR)<<"send resp failed!, fd:"<<fd<<", ret:"<<ret;
            }
            */
            break;
        }

        rpc_pb_cmd_ctx_t *cmd_ctx =new rpc_pb_cmd_ctx_t();
        cmd_ctx->cmd = cmd;
        cmd_ctx->cmd_base = cmd_base;
        ctx->cmd_queue.push(cmd_ctx);
        wakeup_head_n(&ctx->req_wait, ctx->cmd_queue.size());
    }
    ctx->r_stop = 1;
    wakeup_all(&ctx->req_wait);
    return 0;
}

static int proc_rpc_pb_async(conn_info_t *conn)
{
    server_t * server_base= conn->server; 
    rpc_pb_server_t * server = (rpc_pb_server_t *) server_base->extend;

    conet::ObjPool<CmdBase> cmd_base_pool; 

    rpc_pb_conn_asyc_ctx_t a_ctx;
    a_ctx.fd = conn->fd;
    a_ctx.conn = conn;
    a_ctx.server = server;
    a_ctx.server_base = server_base;
    a_ctx.cur_req_num = 0;
    init_wait_queue(&a_ctx.req_wait);
    init_wait_queue(&a_ctx.rsp_wait);
    a_ctx.to_stop = 0;
    a_ctx.w_stop = 0;
    a_ctx.r_stop = 0;
    a_ctx.cmd_base_pool = &cmd_base_pool;

    conet::coroutine_t * r_co = conet::alloc_coroutine((CO_MAIN_FUN *)&proc_rpc_pb_read_co, &a_ctx);
    conet::resume(r_co);

    conet::coroutine_t * w_co = conet::alloc_coroutine((CO_MAIN_FUN *)&proc_rpc_pb_write_co, &a_ctx);
    conet::resume(w_co);

    size_t work_num = 10;
    conet::coroutine_t **work_co = new conet::coroutine_t *[work_num]; 

    for(size_t i=0; i<work_num; ++i) 
    {
        work_co[i] = conet::alloc_coroutine((CO_MAIN_FUN *)&proc_rpc_pb_work_co, &a_ctx);
        conet::resume(work_co[i]);
    }

    conet::wait(r_co);
    conet::free_coroutine(r_co);

    conet::wait(w_co);
    conet::free_coroutine(w_co);

    for(size_t i=0; i<work_num; ++i)  
    {
        conet::wait(work_co[i]);
        conet::free_coroutine(work_co[i]);
    }

    return 0;
}
