/*
 * =====================================================================================
 *
 *       Filename:  pb_rpbc_server.h
 *
 *    Description:  
 *
 *        Version:  1.0
 *        Created:  2014年05月25日 03时18分43秒
 *       Revision:  none
 *       Compiler:  gcc
 *
 *         Author:  piboye
 *   Organization:  
 *
 * =====================================================================================
 */
#ifndef __PB_RPC_SERVER_H_INC__
#define __PB_RPC_SERVER_H_INC__
#include "server_base.h"
#include <string>
#include <map>
#include "svrkit/rpc_base_pb.pb.h"
#include "http_server.h"
#include "pb2json.h"
#include "query_string.h"

namespace conet
{
struct rpc_pb_server_t;
struct rpc_pb_ctx_t
{
    int to_close; // close connection when set 1
    conn_info_t * conn_info;
    rpc_pb_server_t *server;
    conet_rpc_pb::CmdBase *req;
    void * arg;
};

   typedef int (*rpc_pb_callback)(void *, rpc_pb_ctx_t *ctx, std::string * req, std::string *resp, std::string * errmsg);

struct rpc_pb_cmd_t
{
   rpc_pb_callback proc;
   void *arg; 
   std::string name;
};

struct rpc_pb_server_t
{
    struct server_t * server;
    http_server_t *http_server;
    std::string server_name;
    std::map<std::string, rpc_pb_cmd_t> cmd_maps;
};

int get_global_server_cmd(rpc_pb_server_t * server);

int registry_cmd(std::string const & server_name, std::string const & name,  rpc_pb_callback proc, http_callback hproc, void *arg);

int registry_cmd(rpc_pb_server_t *server, std::string const & name,  rpc_pb_callback proc, http_callback hproc, void *arg );


int unregistry_cmd(rpc_pb_server_t *server, std::string const &name);

rpc_pb_cmd_t * get_rpc_pb_cmd(rpc_pb_server_t *server, std::string const &name);



int init_server(
        rpc_pb_server_t *self, 
        std::string const &server_name, 
        char const *ip,
        int port,
        bool use_global_cmd=true
    );

int start_server(rpc_pb_server_t *server);


template <typename R1, typename R2>
R1 get_request_type_from_rpc_pb_func( int (*fun2) (void *arg, rpc_pb_ctx_t *ctx, R1 *req, R2 *resp, std::string *errmsg));

template <typename R1, typename R2>
R2 get_response_type_from_rpc_pb_func( int (*fun2) (void *arg, rpc_pb_ctx_t *ctx, R1 *req, R2*resp, std::string *errmsg));

//server stub
#define RPC_PB_FUNC_WRAP(func, func2) \
int func2(void *arg, rpc_pb_ctx_t *ctx, std::string * req, std::string *resp,  \
        std::string *errmsg) \
{ \
    typeof(conet::get_request_type_from_rpc_pb_func(&func)) req1; \
    if(!req1.ParseFromString(*req)) { \
        return (conet_rpc_pb::CmdBase::ERR_PARSE_REQ_BODY); \
    } \
 \
    typeof(conet::get_response_type_from_rpc_pb_func(&func)) resp1; \
    int ret = 0; \
    ret = func(arg, ctx, &req1, &resp1, errmsg); \
    if (ret) { \
        return ret; \
    } \
    resp1.SerializeToString(resp); \
    return ret; \
} 

#define RPC_PB_HTTP_FUNC_WRAP(cmd, func, func2) \
int func2(void *arg, http_ctx_t *ctx, http_request_t * req, http_response_t *resp) \
{ \
    typeof(conet::get_request_type_from_rpc_pb_func(&func)) req1; \
    int ret = 0; \
    if (req->method == conet::METHOD_GET) {  \
        Json::Value query(Json::objectValue); \
        conet::query_string_to_json(req->query_string.data, req->query_string.len, &query); \
        ret = conet::json2pb(query, &req1, NULL); \
    } else { \
        ret = conet::json2pb(req->body, req->content_length, &req1, NULL); \
    } \
    if(ret) { \
        conet::response_format(resp, 200, "{ret:1, errmsg:\"param error, ret:%d\"}", ret); \
        return -1; \
    } \
    \
    typeof(conet::get_response_type_from_rpc_pb_func(&func)) resp1; \
    ret = 0; \
    rpc_pb_ctx_t pb_ctx;  \
    pb_ctx.to_close = ctx->to_close;  \
    pb_ctx.conn_info = ctx->conn_info;  \
    pb_ctx.server = (conet::rpc_pb_server_t *)ctx->server->extend;  \
    conet_rpc_pb::CmdBase cmdbase; \
    cmdbase.set_type(conet_rpc_pb::CmdBase::REQUEST_TYPE); \
    cmdbase.set_server_name(ctx->server->server_name); \
    cmdbase.set_cmd_name(#cmd); \
    cmdbase.set_seq_id(time(NULL)); \
    pb_ctx.req = &cmdbase; \
    std::string errmsg; \
    ret = func(arg, &pb_ctx, &req1, &resp1, &errmsg); \
    if (ret) { \
        conet::response_format(resp, 200, "{ret:%d, errmsg:\"%s\"}", ret, errmsg.c_str()); \
        return -1; \
    } else {\
        Json::Value root(Json::objectValue); \
        Json::Value body(Json::objectValue);  \
        root["ret"]=0; \
        conet::pb2json(&resp1, &body); \
        root["body"]=body; \
        conet::response_to(resp, 200, root.toStyledString()); \
    } \
    return 0; \
} 



#define REGISTRY_RPC_PB_FUNC(server, cmd, func, arg) \
RPC_PB_FUNC_WRAP(func, rpc_pb_serve_stub_##server##_##cmd) \
RPC_PB_HTTP_FUNC_WRAP(cmd, func, rpc_pb_http_serve_stub_##server##_##cmd) \
int rpc_pb_registry_cmd_##server##_##cmd(); \
static int i_rpc_pb_registry_cmd ## __LINE__ = rpc_pb_registry_cmd_##server##_##cmd();\
int rpc_pb_registry_cmd_##server##_##cmd() \
{ \
    conet::registry_cmd(#server, #cmd, rpc_pb_serve_stub_##server##_##cmd, \
            rpc_pb_http_serve_stub_##server##_##cmd, \
            arg); \
    return 1; \
} \


}
#endif /* end of include guard */ 
