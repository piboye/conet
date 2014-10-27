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
#include "base/incl/net_tool.h"
#include "base/incl/auto_var.h"
#include "svrkit/static_resource.h"
#include "glog/logging.h"
#include "core/incl/wait_queue.h"
#include "base/incl/obj_pool.h"

using namespace conet_rpc_pb;

namespace conet
{

google::protobuf::Message * pb_obj_new(google::protobuf::Message *msg)
{
    return msg->New();
}

static std::map<std::string , StrMap*> *g_server_cmd_maps=NULL;

static std::map<std::string , std::map<std::string, http_cmd_t> > *g_server_http_cmd_maps=NULL;

std::string get_rpc_server_name_default()
{
    if (g_server_cmd_maps && g_server_cmd_maps->size() > 0) {
       return g_server_cmd_maps->begin()->first;
    }
    return std::string();
}

static 
void clear_server_maps(void)
{
    if (g_server_cmd_maps) {
        AUTO_VAR(it, = , g_server_cmd_maps->begin());
        for (; it != g_server_cmd_maps->end(); ++it) {
            delete it->second;
        }
    }
    delete g_server_cmd_maps;
    delete g_server_http_cmd_maps;
    g_server_cmd_maps = NULL;
    g_server_http_cmd_maps = NULL;
}


int rpc_pb_http_call_cb(void *arg, http_ctx_t *ctx, http_request_t * req, http_response_t *resp) 
{ 
    int ret = 0; 

    rpc_pb_cmd_t *self = (rpc_pb_cmd_t *) arg;
    //PbObjPool::node_t *req_node=NULL; 
    //PbObjPool::node_t *rsp_node=NULL; 

    google::protobuf::Message * req1 = NULL;
    //self->m_req_pool.alloc(&req_node, &req1);
    req1 = alloc_pb_obj_from_pool(self->req_msg);

    if (req->method == conet::METHOD_GET) {  
        Json::Value query(Json::objectValue); 
        conet::query_string_to_json(req->query_string.data, req->query_string.len, &query); 
        ret = conet::json2pb(query, req1, NULL); 
    } else { 
        ret = conet::json2pb(req->body, req->content_length, req1, NULL); 
    } 

    if(ret) { 
        conet::response_format(resp, 200, "{\"ret\":1, \"errmsg\":\"param error, ret:%d\"}", ret); 
        //self->m_req_pool.release(req_node, req1);
        free_pb_obj_to_pool(self->req_msg, req1);
        return -1; 
    } 
    
    google::protobuf::Message * rsp1 =  NULL;
    //self->m_rsp_pool.alloc(&rsp_node, &rsp1);
    rsp1 = alloc_pb_obj_from_pool(self->rsp_msg);

    ret = 0; 
    rpc_pb_ctx_t pb_ctx;  
    pb_ctx.to_close = ctx->to_close;  
    pb_ctx.conn_info = ctx->conn_info;  
    pb_ctx.server = (conet::rpc_pb_server_t *)ctx->server->extend;  
    conet_rpc_pb::CmdBase cmdbase; 
    cmdbase.set_type(conet_rpc_pb::CmdBase::REQUEST_TYPE); 
    cmdbase.set_server_name(ctx->server->server_name); 
    cmdbase.set_cmd_name(self->method_name);
    cmdbase.set_seq_id(time(NULL)); 
    pb_ctx.req = &cmdbase; 
    std::string errmsg; 
    ret = self->proc(self->arg, &pb_ctx, req1, rsp1, &errmsg); 
    if (ret) { 
        conet::response_format(resp, 200, "{\"ret\":%d, \"errmsg\":\"%s\"}", ret, errmsg.c_str()); 
        //self->m_req_pool.release(req_node, req1);
        //self->m_rsp_pool.release(rsp_node, rsp1);
        free_pb_obj_to_pool(self->req_msg, req1);
        free_pb_obj_to_pool(self->rsp_msg, rsp1);
        return -1; 
    } else {\
        Json::Value root(Json::objectValue); 
        Json::Value body(Json::objectValue);  
        root["ret"]=0; 
        conet::pb2json(rsp1, &body); 
        root["body"]=body; 
        conet::response_to(resp, 200, root.toStyledString()); 
    } 

    //self->m_req_pool.release(req_node, req1);
    //self->m_rsp_pool.release(rsp_node, rsp1);
    free_pb_obj_to_pool(self->req_msg, req1);
    free_pb_obj_to_pool(self->rsp_msg, rsp1);
    return 0; 
} 

int http_get_rpc_req_default_value(void *arg, http_ctx_t *ctx, http_request_t * req, http_response_t *resp) 
{ 
    rpc_pb_cmd_t *self = (rpc_pb_cmd_t *) arg;
    Json::Value root(Json::objectValue); 
    Json::Value body(Json::objectValue);  
    pb2json(self->req_msg->GetDescriptor(), &body);
    root["ret"]=0; 
    root["req"]=body; 
    conet::response_to(resp, 200, root.toStyledString()); 
    return 0;
}

int http_get_rpc_req_proto(void *arg, http_ctx_t *ctx, http_request_t * req, http_response_t *resp) 
{ 
    rpc_pb_cmd_t *self = (rpc_pb_cmd_t *) arg;
    std::string proto = self->req_msg->GetDescriptor()->DebugString();
    conet::response_to(resp, 200, proto);
    return 0;
}

int http_get_rpc_list(void *arg, http_ctx_t *ctx, http_request_t * req, http_response_t *resp) 
{ 
    rpc_pb_server_t *self = (rpc_pb_server_t *) arg;
    Json::Value root(Json::objectValue); 
    Json::Value list(Json::arrayValue);  


    StrMap::node_type * pn = NULL;
    list_for_each_entry(pn, &self->cmd_maps->m_list, link_to)
    {
        rpc_pb_cmd_t *cmd = container_of(pn, rpc_pb_cmd_t, cmd_map_node);
        list.append(Json::Value(cmd->method_name));
    }

    root["ret"]=0; 
    root["list"]=list; 
    conet::response_to(resp, 200, root.toStyledString()); 
    return 0;
}


int rpc_pb_call_cb(rpc_pb_cmd_t *self, rpc_pb_ctx_t *ctx, 
        std::string *req, std::string *rsp, std::string *errmsg)
{
    //PbObjPool::node_t *req_node=NULL; 
    //PbObjPool::node_t *rsp_node=NULL; 

    google::protobuf::Message * req1 = NULL;
    //self->m_req_pool.alloc(&req_node, &req1);
    req1 = alloc_pb_obj_from_pool(self->req_msg);
    //google::protobuf::Message * req1 = self->req_msg->New();
    if(!req1->ParseFromString(*req)) { 
        //self->m_req_pool.release(req_node, req1);
        free_pb_obj_to_pool(self->req_msg, req1);
        return (conet_rpc_pb::CmdBase::ERR_PARSE_REQ_BODY); 
    } 
 
    google::protobuf::Message * rsp1 = NULL;
    //self->m_rsp_pool.alloc(&rsp_node, &rsp1);
    //google::protobuf::Message * rsp1 = self->rsp_msg->New();
    rsp1 = alloc_pb_obj_from_pool(self->rsp_msg);
    int ret = 0; 
    ret = self->proc(self->arg, ctx, req1, rsp1, errmsg); 
    if (ret) { 
        //self->m_req_pool.release(req_node, req1);
        //self->m_rsp_pool.release(rsp_node, rsp1);
        free_pb_obj_to_pool(self->req_msg, req1);
        free_pb_obj_to_pool(self->rsp_msg, rsp1);
        return ret; 
    } 
    rsp1->SerializeToString(rsp); 
    //self->m_req_pool.release(req_node, req1);
    //self->m_rsp_pool.release(rsp_node, rsp1);
    free_pb_obj_to_pool(self->req_msg, req1);
    free_pb_obj_to_pool(self->rsp_msg, rsp1);
    return ret;
}


int registry_rpc_cmd_http_api(std::string const & method_name, rpc_pb_cmd_t *cmd,
        std::map<std::string, http_cmd_t> *maps) 
{
    {
        http_cmd_t item; 
        item.name = method_name;
        item.proc = rpc_pb_http_call_cb;
        item.arg = cmd;

        maps->insert(std::make_pair(std::string("/rpc/call/") + method_name, item));
    }

    {
        http_cmd_t item; 
        item.name = method_name;
        item.proc = http_get_rpc_req_default_value;
        item.arg = cmd;

        maps->insert(std::make_pair(std::string("/rpc/req_def_val/") + method_name, item));
    }

    {
        http_cmd_t item; 
        item.name = method_name;
        item.proc = http_get_rpc_req_proto;
        item.arg = cmd;

        maps->insert(std::make_pair(std::string("/rpc/req_proto/") + method_name, item));
    }
        
        return 0;
}

static int delete_rpc_pb_cmd_obj(StrMap::node_type *n, void *arg)
{
    rpc_pb_cmd_t *cmd = container_of(n, rpc_pb_cmd_t, cmd_map_node);
    if (cmd) {
        delete cmd;
    }
    return 0;
}

int registry_cmd(std::string const &server_name, rpc_pb_cmd_t  *cmd)
{
    if (NULL == g_server_cmd_maps) {
        g_server_cmd_maps = new typeof(*g_server_cmd_maps);
        g_server_http_cmd_maps = new typeof(*g_server_http_cmd_maps);
        atexit(clear_server_maps);
    }

    std::string const & method_name = cmd->method_name;

    StrMap * maps =  NULL;
    if (g_server_cmd_maps->find(server_name) == g_server_cmd_maps->end()) {
        maps = new StrMap();
        maps->init(100);
        maps->set_destructor_func(&delete_rpc_pb_cmd_obj, NULL);
        g_server_cmd_maps->insert(std::make_pair(server_name, maps));
    } else {
        maps = (*g_server_cmd_maps)[server_name];
    }
    maps->add(&cmd->cmd_map_node);
    //maps.insert(std::make_pair(method_name, cmd));


    { // registry http api
        std::map<std::string, http_cmd_t> * maps = &(*g_server_http_cmd_maps)[server_name];
        registry_rpc_cmd_http_api(method_name, cmd, maps); 
    }
    return 0;
}

int registry_cmd(rpc_pb_server_t *server, rpc_pb_cmd_t *cmd)
{
    std::string const & method_name = cmd->method_name;

    if (server->cmd_maps->find(ref_str(method_name)))
    //if (server->cmd_maps.find(method_name) != server->cmd_maps.end()) 
    {
        return -1;
    }
    
    server->cmd_maps->add(&cmd->cmd_map_node);
    //server->cmd_maps.insert(std::make_pair(method_name, cmd));

    if (server->http_server) {
        AUTO_VAR(maps, =, &server->http_server->cmd_maps);
        registry_rpc_cmd_http_api(method_name, cmd, maps); 
    }

    return 0;
}


int unregistry_cmd(rpc_pb_server_t *server, std::string const &name)
{
    /*
    AUTO_VAR(it, =, server->cmd_maps.find(name));
    if (it == server->cmd_maps.end()) {
        return -1;
    }
    server->cmd_maps.erase(it);
    */

    StrMap::node_type * n = server->cmd_maps->find(ref_str(name));
    if (NULL == n) {
        return -1;
    }

    server->cmd_maps->remove(n);
    server->http_server->cmd_maps.erase(name);
    return 0;
}

rpc_pb_cmd_t * get_rpc_pb_cmd(rpc_pb_server_t *server, std::string const &name)
{
    StrMap::node_type * n = server->cmd_maps->find(ref_str(name));
    if ( NULL == n ) {
        return NULL;
    }
    return container_of(n, rpc_pb_cmd_t, cmd_map_node);
}


int get_global_server_cmd(rpc_pb_server_t * server) 
{
    server->cmd_maps = (*g_server_cmd_maps)[server->server_name];
    server->http_server->cmd_maps = (*g_server_http_cmd_maps)[server->server_name];
    return server->cmd_maps->size();
}

static int proc_rpc_pb(conn_info_t *conn);


int http_get_static_resource(void *arg, http_ctx_t *ctx, http_request_t * req, http_response_t *resp) 
{
    std::string *data = (std::string *) arg; 
    //LOG(ERROR)<<"static file size:"<< data->size(); 
    conet::response_to(resp, 200, *data);
    return 0;
}

#define REGISTRY_STATIC_RESOURCE(http_server, path, res) \
    { \
        http_cmd_t item;  \
        item.name = path; \
        item.proc = http_get_static_resource; \
        item.arg = new std::string(RESOURCE_svrkit_static_##res, sizeof(RESOURCE_svrkit_static_##res)); \
 \
        http_server->cmd_maps.insert(std::make_pair(path, item)); \
    } \

int registry_http_rpc_default_api(rpc_pb_server_t *server)
{
    {
        std::string path = std::string("/rpc/list"); 
        http_cmd_t item; 
        item.name = path;
        item.proc = http_get_rpc_list;
        item.arg = server;

        server->http_server->cmd_maps.insert(std::make_pair(path, item));
    }
    REGISTRY_STATIC_RESOURCE(server->http_server, "/", list_html);
    REGISTRY_STATIC_RESOURCE(server->http_server, "/rpc/list.html", list_html);
    REGISTRY_STATIC_RESOURCE(server->http_server, "/rpc/form.html", form_html);
    REGISTRY_STATIC_RESOURCE(server->http_server, "/js/jquery.js", jquery_js);
    REGISTRY_STATIC_RESOURCE(server->http_server, "/js/bootstrap.min.js", bootstrap_min_js);
    REGISTRY_STATIC_RESOURCE(server->http_server, "/js/qrcode.min.js", qrcode_min_js);
    REGISTRY_STATIC_RESOURCE(server->http_server, "/css/bootstrap.min.css", bootstrap_min_css);

   return 0; 
}

int init_server(
        rpc_pb_server_t *self, 
        std::string const &server_name, 
        char const *ip,
        int port,
        bool use_global_cmd,
        char const * http_ip,
        int http_port
    )
{
    self->server_name =server_name;
    server_t *server_base = new server_t();
    int ret = 0;
    ret = init_server(server_base, ip, port);
    if (ret) {
        delete server_base;
        LOG(ERROR)<<"init server_base in rpc server failed, [ret:"<<ret<<"]";
        return -1;
    }

    self->cmd_maps = NULL;

    self->server = server_base; 

    http_server_t *http_server = new http_server_t();

    if (http_ip == NULL && http_port == 0) {
        http_server->server = server_base; 
        http_server->extend = self;
        self->http_server = http_server;
    } else {
        server_t *server_base2 = new server_t();
        ret = init_server(server_base2, http_ip, http_port);
        if (ret) {
            delete server_base2;
            LOG(ERROR)<<"init server_base in http server failed, [ret:"<<ret<<"]";
            return -1;
        }
        http_server->server = server_base2; 
        http_server->extend = self;
        self->http_server = http_server;
    }

    if (use_global_cmd) {
        get_global_server_cmd(self);
    }
    registry_http_rpc_default_api(self);
    self->async_flag = 0;
    return 0;
}

static int proc_rpc_pb_async(conn_info_t *conn);

int start_server(rpc_pb_server_t *server)
{
    server->server->extend = server;
    if (server->async_flag) {
        server->server->proc = proc_rpc_pb_async;
    } else {
        server->server->proc = proc_rpc_pb;
    }
    if (server->cmd_maps == NULL) {
        return -1;
    }
    int ret =  0;
    ret = start_server(server->server);
    //start_server(server->http_server);
    return ret;
}


static int proc_rpc_pb(conn_info_t *conn)
{
    server_t * server_base= conn->server; 
    rpc_pb_server_t * server = (rpc_pb_server_t *) server_base->extend;
    int max_size = server_base->conf.max_packet_size;
    int ret = 0;
    int fd = conn->fd;

    rpc_pb_ctx_t ctx;

    CmdBase cmd_base;

    CmdBaseResp resp_base;

    ctx.server = server;
    ctx.conn_info = conn;
    ctx.req = &cmd_base;
    ctx.to_close = 0;

    PacketStream stream;
    stream.init(fd, max_size);
    std::vector<char> out_buf;
    while(0 == server_base->to_stop)
    {

        struct pollfd pf = {
            fd: fd,
            events: POLLIN | POLLERR | POLLHUP
        };

        ret = poll( &pf, 1, 1000);
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

        if (cmd_base.server_name() != server->server_name) {
            LOG(ERROR)<< "request server["<<cmd_base.server_name()<<"] nomatch the server["<<server->server_name<<"]";
            break;

        }

        std::string const & cmd_name = cmd_base.cmd_name();
        rpc_pb_cmd_t * cmd = get_rpc_pb_cmd(server, cmd_name);
        if (NULL == cmd) {
            // not unsuppend cmd;
            LOG(ERROR)<< "unsuppend cmd, server:"<<server->server_name
                <<"cmd:"<<cmd_name;

            cmd_base.set_ret(CmdBase::ERR_UNSUPPORED_CMD);
            cmd_base.set_errmsg("unsuppored cmd");
            ret = send_pb_obj(fd, cmd_base, &out_buf);
            if (ret <= 0) {
                LOG(ERROR)<<"send resp failed!, fd:"<<fd<<", ret:"<<ret;
            }
            break;
        }

        std::string * req = cmd_base.mutable_body();


        int retcode = 0;
        std::string *rsp = cmd_base.mutable_body();
        std::string *errmsg = cmd_base.mutable_errmsg();
        retcode = rpc_pb_call_cb(cmd, &ctx, req, rsp, errmsg);

        cmd_base.set_type(CmdBase::RESPONSE_TYPE);
        cmd_base.set_ret(retcode);

        ret = send_pb_obj(fd, cmd_base, &out_buf);
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

        if (cmd_base->server_name() != server->server_name) {
            LOG(ERROR)<< "request server["<<cmd_base->server_name()<<"] nomatch the server["<<server->server_name<<"]";
            delete cmd_base;
            break;

        }

        std::string const & cmd_name = cmd_base->cmd_name();
        rpc_pb_cmd_t * cmd = get_rpc_pb_cmd(server, cmd_name);
        if (NULL == cmd) {
            // not unsuppend cmd;
            LOG(ERROR)<< "unsuppend cmd, server:"<<server->server_name
                <<"cmd:"<<cmd_name;

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
    cmd_base_pool.init();

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


int stop_server(rpc_pb_server_t *server, int wait)
{
    int ret = 0;
    LOG(INFO)<<"["<<server->server_name<<"] stop rpc main server";
    ret = stop_server(server->server, wait);
    if (ret) {
        LOG(INFO)<<"["<<server->server_name<<"] stop rpc main server success";
    } else {
        LOG(INFO)<<"["<<server->server_name<<"] stop rpc main server failed, [ret:"<<ret<<"]";
    }
    
    LOG(INFO)<<"["<<server->server_name<<"] stop rpc http server";
    ret = stop_server(server->http_server, wait);
    if (ret) {
        LOG(ERROR)<<"["<<server->server_name<<"] stop rpc http server failed, [ret:"<<ret<<"]";
    } else {
        LOG(INFO)<<"["<<server->server_name<<"] stop rpc http server success";
    }

    LOG(INFO)<<"["<<server->server_name<<"] stop rpc finished";
    return ret;
}

}
