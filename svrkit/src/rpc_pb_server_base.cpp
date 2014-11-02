/*
 * =====================================================================================
 *
 *       Filename:  rpc_pb_server_base.cpp
 *
 *    Description:  
 *
 *        Version:  1.0
 *        Created:  2014年11月02日 05时54分56秒
 *       Revision:  none
 *       Compiler:  gcc
 *
 *         Author:  piboye
 *   Organization:  
 *
 * =====================================================================================
 */
#include <stdlib.h>
#include <map>
#include <string>

#include "rpc_pb_server_base.h"
#include "rpc_pb_server_base_impl.h"
#include "base/incl/auto_var.h"
#include "glog/logging.h"
#include "base/incl/pb2json.h"
#include "base/incl/net_tool.h"
#include "base/incl/query_string.h"
#include "svrkit/static_resource.h"

namespace conet
{

static std::map<std::string, rpc_pb_cmd_t*> *g_server_cmd_maps=NULL;
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
    g_server_cmd_maps = NULL;
}


int global_registry_cmd(rpc_pb_cmd_t  *cmd)
{
    if (NULL == g_server_cmd_maps) {
        g_server_cmd_maps = new typeof(*g_server_cmd_maps);
        atexit(clear_server_maps);
    }

    std::string const & method_name = cmd->method_name;

    if (g_server_cmd_maps->find(method_name) != g_server_cmd_maps->end())
    {
        LOG(ERROR)<<"duplicate cmd:"<<method_name<<" has been registried!";
        return -1;
    }
    g_server_cmd_maps->insert(std::make_pair(method_name, cmd));

    return 0;
}

static int delete_rpc_pb_cmd_obj(void *arg, StrMap::node_type *n)
{
    rpc_pb_cmd_t *cmd = container_of(n, rpc_pb_cmd_t, cmd_map_node);
    if (cmd) {
        delete cmd;
    }
    return 0;
}

rpc_pb_server_base_t::rpc_pb_server_base_t()
{
    this->cmd_maps.init(100);
    this->cmd_maps.set_destructor_func(&delete_rpc_pb_cmd_obj, NULL);
}

rpc_pb_cmd_t * rpc_pb_server_base_t::get_rpc_pb_cmd(char const *method_name, size_t len)
{
    StrMap::node_type * n = this->cmd_maps.find(ref_str(method_name, len));
    if ( NULL == n ) {
        return NULL;
    }
    return container_of(n, rpc_pb_cmd_t, cmd_map_node);
}

int rpc_pb_server_base_t::get_global_server_cmd()
{
    if (NULL == g_server_cmd_maps) {
        LOG(ERROR)<<"no cmd has been registried!";
        return -1;
    }

    AUTO_VAR(it, = , g_server_cmd_maps->begin());
    for (; it != g_server_cmd_maps->end(); ++it) 
    {
        rpc_pb_cmd_t *cmd = it->second;
        rpc_pb_cmd_t *cmd2 = cmd->clone(); 
        this->cmd_maps.add(&cmd2->cmd_map_node);
    }

    return this->cmd_maps.size();
}

int rpc_pb_http_call_cb(void *arg, http_ctx_t *ctx, http_request_t * req, http_response_t *resp) 
{ 
    int ret = 0; 

    rpc_pb_cmd_t *self = (rpc_pb_cmd_t *) arg;

    google::protobuf::Message * req1 = self->req_msg;
    if (req1 ) {
        req1  = self->req_pool.alloc();
        if (req->method == conet::METHOD_GET) {  
            Json::Value query(Json::objectValue); 
            conet::query_string_to_json(req->query_string.data, req->query_string.len, &query); 
            ret = conet::json2pb(query, req1, NULL); 
        } else { 
            ret = conet::json2pb(req->body, req->content_length, req1, NULL); 
        } 


        if(ret) { 
            conet::response_format(resp, 200, "{\"ret\":1, \"errmsg\":\"param error, ret:%d\"}", ret); 
            self->req_pool.release(req1);
            return -1; 
        } 
    }
    
    google::protobuf::Message * rsp1 = self->rsp_msg; 

    if (rsp1) {
        rsp1 = self->rsp_pool.alloc();
    }

    ret = 0; 
    rpc_pb_ctx_t pb_ctx;  
    pb_ctx.to_close = ctx->to_close;  
    pb_ctx.conn_info = ctx->conn_info;  
    pb_ctx.server = ctx->server;
    conet_rpc_pb::CmdBase cmdbase; 
    cmdbase.set_type(conet_rpc_pb::CmdBase::REQUEST_TYPE); 
    cmdbase.set_cmd_name(self->method_name);
    cmdbase.set_seq_id(time(NULL)); 

    pb_ctx.req = &cmdbase; 
    std::string errmsg; 

    AUTO_VAR(obj_mgr, =, self->obj_mgr);
    if (obj_mgr) {
        void *obj = obj_mgr->alloc();
        ret = self->proc(obj, &pb_ctx, req1, rsp1, &errmsg); 
        obj_mgr->free(obj);
    } else {
        ret = self->proc(self->arg, &pb_ctx, req1, rsp1, &errmsg); 
    }

    Json::Value root(Json::objectValue); 
    Json::Value body(Json::objectValue);  
    if (!errmsg.empty()) {
        root["errmsg"] = errmsg;
    }
    if (rsp1) {
        conet::pb2json(rsp1, &body); 
        root["body"]=body; 
    }
    conet::response_to(resp, 200, root.toStyledString()); 

    if (req1) self->req_pool.release(req1);
    if (rsp1) self->rsp_pool.release(rsp1);

    if (ret) {
        return -1;
    }
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
    rpc_pb_server_base_t *self = (rpc_pb_server_base_t *) arg;

    Json::Value root(Json::objectValue); 
    Json::Value list(Json::arrayValue);  


    StrMap::node_type * pn = NULL;
    list_for_each_entry(pn, &self->cmd_maps.m_list, link_to)
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
    int ret = 0; 

    google::protobuf::Message * req1 = self->req_msg;
    if (req1) {
        req1 = self->req_pool.alloc();
        if(!req1->ParseFromString(*req)) { 
            self->req_pool.release(req1);
            return (conet_rpc_pb::CmdBase::ERR_PARSE_REQ_BODY); 
        } 
    }
 
    google::protobuf::Message * rsp1 = self->rsp_msg;

    if (rsp1) {
        rsp1 = self->rsp_pool.alloc();
    }

    AUTO_VAR(obj_mgr, =, self->obj_mgr);
    if (obj_mgr) {
        void *obj = obj_mgr->alloc();
        ret = self->proc(obj, ctx, req1, rsp1, errmsg); 
        obj_mgr->free(obj);
    } else {
        ret = self->proc(self->arg, ctx, req1, rsp1, errmsg); 
    }

    if (rsp1) { 
        rsp1->SerializeToString(rsp); 
    }

    if (req1) 
        self->req_pool.release(req1);

    if (rsp1) 
        self->rsp_pool.release(rsp1);

    return ret;
}

int rpc_pb_server_base_t::registry_rpc_cmd_http_api(
        http_server_t * http_server, 
        std::string const & method_name, 
        rpc_pb_cmd_t *cmd, 
        std::string const & base_path)
{
    std::map<std::string, http_cmd_t> *maps = &http_server->cmd_maps;

    {
        http_cmd_t item; 
        item.name = method_name;
        item.proc = rpc_pb_http_call_cb;
        item.arg = cmd;

        maps->insert(std::make_pair(base_path + "/rpc/call/" + method_name, item));
    }

    {
        http_cmd_t item; 
        item.name = method_name;
        item.proc = http_get_rpc_req_default_value;
        item.arg = cmd;

        maps->insert(std::make_pair(base_path + "/rpc/req_def_val/" + method_name, item));
    }

    {
        http_cmd_t item; 
        item.name = method_name;
        item.proc = http_get_rpc_req_proto;
        item.arg = cmd;

        maps->insert(std::make_pair(base_path + "/rpc/req_proto/" + method_name, item));
    }
        
        return 0;
}

int http_get_static_resource(void *arg, http_ctx_t *ctx, http_request_t * req, http_response_t *resp) 
{
    std::string *data = (std::string *) arg; 
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

int rpc_pb_server_base_t::registry_http_rpc_default_api(http_server_t *http_server, std::string const &base_path)
{
    {
        std::string path = base_path + "/rpc/list"; 
        http_cmd_t item; 
        item.name = path;
        item.proc = http_get_rpc_list;
        item.arg = this;

        http_server->cmd_maps.insert(std::make_pair(path, item));
    }

    REGISTRY_STATIC_RESOURCE(http_server, base_path + "/", list_html);
    REGISTRY_STATIC_RESOURCE(http_server, base_path + "/rpc/list.html", list_html);
    REGISTRY_STATIC_RESOURCE(http_server, base_path + "/rpc/form.html", form_html);
    REGISTRY_STATIC_RESOURCE(http_server, base_path + "/js/jquery.js", jquery_js);
    REGISTRY_STATIC_RESOURCE(http_server, base_path + "/js/bootstrap.min.js", bootstrap_min_js);
    REGISTRY_STATIC_RESOURCE(http_server, base_path + "/js/qrcode.min.js", qrcode_min_js);
    REGISTRY_STATIC_RESOURCE(http_server, base_path + "/css/bootstrap.min.css", bootstrap_min_css);

   return 0; 
}


int rpc_pb_server_base_t::registry_all_rpc_http_api(http_server_t *http_server, std::string const &base_path)
{
    int ret = 0;
    ret = registry_http_rpc_default_api(http_server, base_path);

    list_head * list = &this->cmd_maps.m_list;
    StrMap::node_type *node = NULL;
    list_for_each_entry(node, list, link_to) 
    {
        rpc_pb_cmd_t *cmd = container_of(node, rpc_pb_cmd_t, cmd_map_node);
        registry_rpc_cmd_http_api(http_server, cmd->method_name, cmd, base_path); 
    }
    return 0;
}

}
