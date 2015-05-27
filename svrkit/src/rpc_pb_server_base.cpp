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
#include <unordered_map>

#include "glog/logging.h"

#include "rpc_pb_server_base.h"
#include "rpc_pb_server_base_impl.h"
#include "svrkit/static_resource.h"

#include "base/auto_var.h"
#include "base/pb2json.h"
#include "base/net_tool.h"
#include "base/query_string.h"
#include "thirdparty/gflags/gflags.h"

using namespace std;

namespace conet
{
DEFINE_bool(log_failed_rpc, true, "log failed rpc call");

static unordered_map<string, unordered_map<string, rpc_pb_cmd_t*> * >  *g_server_cmd_maps=NULL;

static 
void clear_server_maps(void)
{
    auto cmd_maps = g_server_cmd_maps;
    g_server_cmd_maps = NULL;
    if (cmd_maps) {
        for (auto cmd_map : *cmd_maps) 
        {
            if (cmd_map.second == NULL) continue;
            for (auto cmd : *cmd_map.second) 
            {
                delete cmd.second;
            }
            delete cmd_map.second;
        }
    }
    delete cmd_maps;
}


int global_registry_cmd(std::string const &server_name, rpc_pb_cmd_t  *cmd)
{
    if (NULL == g_server_cmd_maps) {
        g_server_cmd_maps = new typeof(*g_server_cmd_maps);
        atexit(clear_server_maps);
    }

    std::string const & method_name = cmd->method_name;
    auto server_map = (*g_server_cmd_maps)[server_name];
    if (server_map == NULL)
    {
        server_map = new typeof(*server_map);
        (*g_server_cmd_maps)[server_name] = server_map;
    }


    if (server_map->find(method_name) != server_map->end())
    {
        LOG(FATAL)<<"duplicate cmd:"<<method_name<<" has been registried!"
            <<" in [server:"<<server_name<<"]";
        return -1;
    }
    server_map->insert(std::make_pair(method_name, cmd));

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
    this->cmd_maps.init(1000);
    this->cmd_maps.set_destructor_func(&delete_rpc_pb_cmd_obj, NULL);
    this->cmd_id_maps.init(1000);
}

rpc_pb_cmd_t * rpc_pb_server_base_t::get_rpc_pb_cmd(char const *method_name, size_t len)
{
    StrMap::node_type * n = this->cmd_maps.find(ref_str(method_name, len));
    if ( NULL == n ) {
        return NULL;
    }
    return container_of(n, rpc_pb_cmd_t, cmd_map_node);
}

rpc_pb_cmd_t * rpc_pb_server_base_t::get_rpc_pb_cmd(uint64_t cmd_id)
{
    AUTO_VAR(n, =, this->cmd_id_maps.find(cmd_id));
    if ( NULL == n ) {
        return NULL;
    }
    return container_of(n, rpc_pb_cmd_t, cmd_id_map_node);
}

int rpc_pb_server_base_t::get_global_server_cmd(std::string const & server_name)
{
    if (NULL == g_server_cmd_maps) {
        LOG(ERROR)<<"no cmd has been registried in [server:"<<server_name<<"]";
        return -1;
    }

    auto server_map = (*g_server_cmd_maps)[server_name];
    if (server_map == NULL)
    {
        LOG(ERROR)<<"no cmd has been registried in [server:"<<server_name<<"]";
        return -2;
    }
    AUTO_VAR(it, = , server_map->begin());
    for (; it != server_map->end(); ++it) 
    {
        rpc_pb_cmd_t *cmd = it->second;
        rpc_pb_cmd_t *cmd2 = cmd->clone(); 
        this->cmd_maps.add(&cmd2->cmd_map_node);
        if (cmd2->cmd_id > 0) {
            this->cmd_id_maps.add(&cmd2->cmd_id_map_node);
        }
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

    cmd_base_t & cmdbase = pb_ctx.cmd_base;
    cmdbase.type = conet_rpc_pb::CmdBase::REQUEST_TYPE; 
    init_ref_str(&cmdbase.cmd_name, self->method_name);
    cmdbase.seq_id = (time(NULL)); 

    std::string errmsg; 

    AUTO_VAR(obj_mgr, =, self->obj_mgr);
    if (obj_mgr) {
        void *obj = obj_mgr->alloc();
        ret = self->proc(obj, &pb_ctx, req1, rsp1, &errmsg); 
        obj_mgr->free(obj);
    } else {
        ret = self->proc(self->arg, &pb_ctx, req1, rsp1, &errmsg); 
    }

    std::string out_txt="{\"ret\":";
    char tmp[30];
    snprintf(tmp, sizeof(tmp), "%d", ret);
    out_txt.append(tmp);

    if (!errmsg.empty()) {
        out_txt.append(",\"errmsg\":");
        out_txt.append(errmsg);
    }

    if (rsp1) {
        out_txt.append(",\"body\":");
        conet::pb2json(rsp1, &out_txt); 
    }

    out_txt.append("}");
    conet::response_to(resp, 200, out_txt);

    //Json::FastWriter writer;
    //conet::response_to(resp, 200, writer.write(root));
    //conet::response_to(resp, 200, root.toStyledString());

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
    root["ret"]=0; 
    if (self->req_msg) {
        pb2json(self->req_msg->GetDescriptor(), &body);
        root["req"].swap(body); 
    } else {
        //没有请求体
        root["req"]="";
    }

    //Json::FastWriter writer;
    //conet::response_to(resp, 200, writer.write(root));
    conet::response_to(resp, 200, root.toStyledString());
    return 0;
}

int http_get_rpc_req_proto(void *arg, http_ctx_t *ctx, http_request_t * req, http_response_t *resp) 
{
    rpc_pb_cmd_t *self = (rpc_pb_cmd_t *) arg;
    std::string proto="none request boyd";
    if (self->req_msg) {
        proto = self->req_msg->GetDescriptor()->DebugString();
    }

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
        Json::Value cmd_val(cmd->method_name);
        list[list.size()].swap(cmd_val);
    }

    root["ret"]=0; 
    root["list"].swap(list);
    //Json::FastWriter writer;
    //conet::response_to(resp, 200, writer.write(root));
    conet::response_to(resp, 200, root.toStyledString());
    return 0;
}

int rpc_pb_call_cb(rpc_pb_cmd_t *self, rpc_pb_ctx_t *ctx, 
        ref_str_t req, google::protobuf::Message **rsp, std::string *errmsg)
{
    int ret = 0; 

    google::protobuf::Message * req1 = self->req_msg;
    if (req1) {
        req1 = self->req_pool.alloc();
        if(!req1->ParseFromArray(req.data, req.len)) { 
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
        *rsp =rsp1;
    }

    if (req1) 
        self->req_pool.release(req1);

    if (ret != 0 && FLAGS_log_failed_rpc)
    {
        LOG(ERROR)<<"rpc call failed, "
            "[method_name:"<<self->method_name<<"]"
            "[cmd_id:"<<self->cmd_id<<"]"
            "[seq_id:"<<ctx->cmd_base.seq_id<<"]"
            "[ret:"<<ret<<"]"
            "[errmsg:"<<*errmsg<<"]"
            "[req:"<<req1->ShortDebugString()<<"]"
            "[rsp:"<<rsp1->ShortDebugString()<<"]"
            ;
    }
    return ret;
}

int rpc_pb_server_base_t::registry_rpc_cmd_http_api(
        http_server_t * http_server, 
        std::string const & method_name, 
        rpc_pb_cmd_t *cmd, 
        std::string const & base_path)
{
    //调用rpc命令
    http_server->registry_cmd(base_path + "/rpc/call/" + method_name,
            rpc_pb_http_call_cb, cmd, NULL);

    //获取参数默认值
    http_server->registry_cmd(base_path + "/rpc/req_def_val/" + method_name,
            http_get_rpc_req_default_value, cmd, NULL);

    //获取参数原型
    http_server->registry_cmd(base_path + "/rpc/req_proto/" + method_name,
            http_get_rpc_req_proto, cmd, NULL);

    return 0;
}

int http_get_static_resource(void *arg, http_ctx_t *ctx, http_request_t * req, http_response_t *resp) 
{
    std::string *data = (std::string *) arg; 
    conet::response_to(resp, 200, *data);
    return 0;
}

static void delete_string(void *arg)
{
   delete (std::string *)(arg);
}

#define REGISTRY_STATIC_RESOURCE(http_server, path, res) \
    { \
        std::string *arg = new std::string(RESOURCE_svrkit_static_##res, sizeof(RESOURCE_svrkit_static_##res)); \
        int ret = http_server->registry_cmd(path, http_get_static_resource,  arg, \
                &delete_string); \
        if (ret) { \
            delete_string(arg); \
        } \
    } \

int rpc_pb_server_base_t::registry_http_rpc_default_api(http_server_t *http_server, std::string const &base_path)
{
    http_server->registry_cmd(base_path + "/rpc/list", http_get_rpc_list, this, NULL);

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
    if (ret) {
        return ret;
    }

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
