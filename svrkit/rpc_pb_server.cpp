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
#include "rpc_pb_server.h"
#include "net_tool.h"
#include "core/incl/auto_var.h"
#include "svrkit/static_resource.h"
#include "glog/logging.h"

using namespace conet_rpc_pb;

namespace conet
{

static std::map<std::string , std::map<std::string, rpc_pb_cmd_t*> > *g_server_cmd_maps=NULL;

static std::map<std::string , std::map<std::string, http_cmd_t> > *g_server_http_cmd_maps=NULL;

static 
void clear_g_server_maps(void)
{
    delete g_server_cmd_maps;
    delete g_server_http_cmd_maps;
    g_server_cmd_maps = NULL;
    g_server_http_cmd_maps = NULL;
}


int rpc_pb_http_call_cb(void *arg, http_ctx_t *ctx, http_request_t * req, http_response_t *resp) 
{ 
    rpc_pb_cmd_t *self = (rpc_pb_cmd_t *) arg;

    google::protobuf::Message * req1 = self->req_msg->New();
    int ret = 0; 
    if (req->method == conet::METHOD_GET) {  
        Json::Value query(Json::objectValue); 
        conet::query_string_to_json(req->query_string.data, req->query_string.len, &query); 
        ret = conet::json2pb(query, req1, NULL); 
    } else { 
        ret = conet::json2pb(req->body, req->content_length, req1, NULL); 
    } 

    if(ret) { 
        conet::response_format(resp, 200, "{\"ret\":1, \"errmsg\":\"param error, ret:%d\"}", ret); 
        delete req1;
        return -1; 
    } 
    
    google::protobuf::Message * rsp1 = self->rsp_msg->New();
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
        delete req1;
        delete rsp1;
        return -1; 
    } else {\
        Json::Value root(Json::objectValue); 
        Json::Value body(Json::objectValue);  
        root["ret"]=0; 
        conet::pb2json(rsp1, &body); 
        root["body"]=body; 
        conet::response_to(resp, 200, root.toStyledString()); 
    } 
    delete req1;
    delete rsp1;
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


    AUTO_VAR(it, =, self->cmd_maps.begin());
    for(; it != self->cmd_maps.end(); ++it) {
        list.append(Json::Value(it->first));        
    }

    root["ret"]=0; 
    root["list"]=list; 
    conet::response_to(resp, 200, root.toStyledString()); 
    return 0;
}


int rpc_pb_call_cb(rpc_pb_cmd_t *self, rpc_pb_ctx_t *ctx, 
        std::string *req, std::string *rsp, std::string *errmsg)
{
    google::protobuf::Message * req1 = self->req_msg->New();
    if(!req1->ParseFromString(*req)) { 
        delete req1;
        return (conet_rpc_pb::CmdBase::ERR_PARSE_REQ_BODY); 
    } 
 
    google::protobuf::Message * rsp1 = self->rsp_msg->New();
    int ret = 0; 
    ret = self->proc(self->arg, ctx, req1, rsp1, errmsg); 
    if (ret) { 
        delete req1;
        delete rsp1;
        return ret; 
    } 
    rsp1->SerializeToString(rsp); 
    delete req1;
    delete rsp1;
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

int registry_cmd(std::string const &server_name, rpc_pb_cmd_t  *cmd)
{
    if (NULL == g_server_cmd_maps) {
        g_server_cmd_maps = new typeof(*g_server_cmd_maps);
        g_server_http_cmd_maps = new typeof(*g_server_http_cmd_maps);
        atexit(clear_g_server_maps);
    }

    std::string const & method_name = cmd->method_name;

    std::map<std::string, rpc_pb_cmd_t*> & maps = (*g_server_cmd_maps)[server_name];
    maps.insert(std::make_pair(method_name, cmd));


    { // registry http api
        std::map<std::string, http_cmd_t> * maps = &(*g_server_http_cmd_maps)[server_name];
        registry_rpc_cmd_http_api(method_name, cmd, maps); 
    }
    return 0;
}

int registry_cmd(rpc_pb_server_t *server, rpc_pb_cmd_t *cmd)
{
    std::string const & method_name = cmd->method_name;

    if (server->cmd_maps.find(method_name) != server->cmd_maps.end()) {
        return -1;
    }
    server->cmd_maps.insert(std::make_pair(method_name, cmd));

    if (server->http_server) {
        AUTO_VAR(maps, =, &server->http_server->cmd_maps);
        registry_rpc_cmd_http_api(method_name, cmd, maps); 
    }

    return 0;
}


int unregistry_cmd(rpc_pb_server_t *server, std::string const &name)
{
    AUTO_VAR(it, =, server->cmd_maps.find(name));
    if (it == server->cmd_maps.end()) {
        return -1;
    }
    server->cmd_maps.erase(it);
    server->http_server->cmd_maps.erase(name);
    return 0;
}

rpc_pb_cmd_t * get_rpc_pb_cmd(rpc_pb_server_t *server, std::string const &name)
{
    AUTO_VAR(it, =, server->cmd_maps.find(name));
    if (it == server->cmd_maps.end()) {
        return NULL;
    }
    return it->second;
}


int get_global_server_cmd(rpc_pb_server_t * server) 
{
    server->cmd_maps = (*g_server_cmd_maps)[server->server_name];
    server->http_server->cmd_maps = (*g_server_http_cmd_maps)[server->server_name];
    return server->cmd_maps.size();
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
    REGISTRY_STATIC_RESOURCE(server->http_server, "/css/bootstrap.min.css", bootstrap_min_css);

   return 0; 
}

int init_server(
        rpc_pb_server_t *self, 
        std::string const &server_name, 
        char const *ip,
        int port,
        bool use_global_cmd
    )
{
    self->server_name =server_name;
    server_t *server_base = new server_t();
    int ret = 0;
    ret = init_server(server_base, ip, port);
    if (ret) {
        delete server_base;
        LOG(ERROR)<<"init server_baser in rpc server failed, [ret:"<<ret<<"]";
        return -1;
    }
    self->server = server_base; 

    /*
    server_t *server_base2 = new server_t();
    ret = init_server(server_base2, ip, port+10000);
    if (ret) {
        delete server_base2;
        LOG(ERROR)<<"init server_baser in http server failed, [ret:"<<ret<<"]";
        return -1;
    }
    */

    http_server_t *http_server = new http_server_t();
    http_server->server = server_base; 
    http_server->extend = self;
    self->http_server = http_server;
    if (use_global_cmd) {
        get_global_server_cmd(self);
    }
    registry_http_rpc_default_api(self);
    return 0;
}

int start_server(rpc_pb_server_t *server)
{
    server->server->extend = server;
    server->server->proc = proc_rpc_pb;
    if (server->cmd_maps.empty() ) {
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
    int max_size = server_base->max_packet_size;
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
    //uint32_t out_len = max_size;
    std::string errmsg;
    std::string resp;
    do
    {

        struct pollfd pf = {
            fd: fd,
            events: POLLIN | POLLERR | POLLHUP
        };

        poll( &pf, 1, 1000);

        if (pf.revents & POLLERR) {
            break;
        }

        if (!(pf.revents & POLLIN)) {
            continue;
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

        std::string cmd_name = cmd_base.cmd_name();
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

        resp.resize(0);
        errmsg.resize(0);

        int retcode = 0;
        retcode = rpc_pb_call_cb(cmd, &ctx, req, &resp, &errmsg);

        cmd_base.set_type(CmdBase::RESPONSE_TYPE);
        cmd_base.set_ret(retcode);
        if (!errmsg.empty()) cmd_base.set_errmsg(errmsg);
        if (!resp.empty()) cmd_base.set_body(resp);

        ret = send_pb_obj(fd, cmd_base, &out_buf);
        if (ret <=0) {
            // send data failed;
            LOG(ERROR)<<"send resp failed!, fd:"<<fd<<", ret:"<<ret;
            break; 
        }
        if (ctx.to_close) {
            break;
        }
    } while(1);
    return 0;
}

}
