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
#include "server/server_base.h"
#include <string>
#include <map>
#include "server/rpc_base_pb.pb.h"
namespace conet
{
struct rpc_pb_server_t;
struct rpc_pb_ctx_t
{
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
    std::string server_name;
    std::map<std::string, rpc_pb_cmd_t> cmd_maps;
};

int get_global_server_cmd(rpc_pb_server_t * server);

int registry_cmd(std::string const & server_name, std::string const & name,  rpc_pb_callback proc, void *arg );

int registry_cmd( rpc_pb_server_t *server, std::string const & name,  rpc_pb_callback proc, void *arg );

int unregistry_cmd(rpc_pb_server_t *server, std::string const &name);

rpc_pb_cmd_t * get_rpc_pb_cmd(rpc_pb_server_t *server, std::string const &name);

rpc_pb_cmd_t * get_rpc_pb_cmd(rpc_pb_server_t *server, std::string const &name);


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


#define REGISTRY_RPC_PB_FUNC(server, cmd, func, arg) \
RPC_PB_FUNC_WRAP(func, rpc_pb_serve_stub_##server##_##cmd) \
int rpc_pb_registry_cmd_##server##_##cmd(); \
static int i_rpc_pb_registry_cmd ## __LINE__ = rpc_pb_registry_cmd_##server##_##cmd();\
int rpc_pb_registry_cmd_##server##_##cmd() \
{ \
    conet::registry_cmd(#server, #cmd, rpc_pb_serve_stub_##server##_##cmd, arg); \
    return 1; \
} \

//client 
int rpc_pb_call_impl(char const *ip, int port, 
        std::string const &server_name,
        std::string const &cmd_name,
        std::string const &req, std::string *resp, std::string *errmsg);

template <typename ReqT, typename RespT>
int rpc_pb_call(char const *ip, int port, 
        std::string const &server_name,
        std::string const &cmd_name,
        ReqT const *a_req, RespT *a_resp, std::string *errmsg=NULL)
{
    std::string req;
    if (!a_req->SerializeToString(&req)) {
        return -8;
    }
    std::string resp;
    int ret = 0;

    ret = rpc_pb_call_impl(ip, port, server_name, cmd_name, req, &resp, errmsg);

    if (ret) {
        return ret;
    }

    if (a_resp) {
        if (!a_resp->ParseFromString(resp)) {
            return -7;
        }
    }
    return ret;
}

}
#endif /* end of include guard */ 
