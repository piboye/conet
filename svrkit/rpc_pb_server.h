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
#include "google/protobuf/descriptor.h"
#include "google/protobuf/message.h"
#include "obj_pool.h"
#include "str_map.h"

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

typedef int (*rpc_pb_callback)(void *, rpc_pb_ctx_t *ctx, \
        google::protobuf::Message * req, google::protobuf::Message *resp,  \
        std::string * errmsg);

template <typename T1, typename R1, typename R2>
R1 get_request_type_from_rpc_pb_func( int (*fun2) (T1 *arg, rpc_pb_ctx_t *ctx, R1 *req, R2 *resp, std::string *errmsg));

template <typename T1, typename R1, typename R2>
R2 get_response_type_from_rpc_pb_func( int (*fun2) (T1 *arg, rpc_pb_ctx_t *ctx, R1 *req, R2*resp, std::string *errmsg));


std::string get_rpc_server_name_default();

struct rpc_pb_cmd_t
{

   rpc_pb_callback proc;

   void *arg; 
   
   std::string method_name;
   StrMap::StrNode cmd_map_node;

   google::protobuf::Message * req_msg;
   google::protobuf::Message * rsp_msg;
   ObjPoll<google::protobuf::Message> m_req_pool; 
   ObjPoll<google::protobuf::Message> m_rsp_pool; 
};


struct rpc_pb_server_t
{
    struct server_t * server;
    http_server_t *http_server;
    std::string server_name;
    StrMap *cmd_maps;
    //std::map<std::string, rpc_pb_cmd_t*> cmd_maps;
};

int get_global_server_cmd(rpc_pb_server_t * server);

int registry_cmd(std::string const &server_name, rpc_pb_cmd_t  *cmd);

int registry_cmd(rpc_pb_server_t *server, rpc_pb_cmd_t *cmd);


int unregistry_cmd(rpc_pb_server_t *server, std::string const &name);

rpc_pb_cmd_t * get_rpc_pb_cmd(rpc_pb_server_t *server, std::string const &name);



int init_server(
        rpc_pb_server_t *self, 
        std::string const &server_name, 
        char const *ip,
        int port,
        bool use_global_cmd=true,
        char const *http_ip=NULL,
        int http_port=0
    );

int start_server(rpc_pb_server_t *server);

int stop_server(rpc_pb_server_t *server, int wait=0);

google::protobuf::Message * pb_obj_new(google::protobuf::Message *msg);

template <typename T1, typename R1, typename R2>
int registry_rpc_pb_cmd(std::string const &server_name, std::string const &method_name,
        int (*func) (T1 *, rpc_pb_ctx_t *ctx, R1 *req, R2*rsp, std::string *errmsg), void *arg)
{
    rpc_pb_cmd_t * cmd = new rpc_pb_cmd_t(); 
    cmd->method_name = method_name; 
    cmd->cmd_map_node.init(cmd->method_name.c_str(), cmd->method_name.size());

    cmd->req_msg = new typeof(R1);
    cmd->rsp_msg = new typeof(R2);

    cmd->m_req_pool.init(conet::NewPermanentClosure(pb_obj_new, cmd->req_msg));
    cmd->m_rsp_pool.init(conet::NewPermanentClosure(pb_obj_new, cmd->rsp_msg));

    cmd->proc = (rpc_pb_callback)(func); 
    cmd->arg = (void *)arg; 
    conet::registry_cmd(server_name, cmd); 
    return 1; 
}

template <typename T1, typename R1, typename R2>
int registry_rpc_pb_cmd(std::string const &server_name, std::string const &method_name,
        int (T1::*func) (rpc_pb_ctx_t *ctx, R1 *req, R2*rsp, std::string *errmsg), T1* arg)
{
    rpc_pb_cmd_t * cmd = new rpc_pb_cmd_t(); 
    cmd->method_name = method_name; 
    cmd->cmd_map_node.init(cmd->method_name.c_str(), cmd->method_name.size());

    cmd->req_msg = new typeof(R1);
    cmd->rsp_msg = new typeof(R2);

    cmd->m_req_pool.init(conet::NewPermanentClosure(pb_obj_new, cmd->req_msg));
    cmd->m_rsp_pool.init(conet::NewPermanentClosure(pb_obj_new, cmd->rsp_msg));

    //cmd->proc = (rpc_pb_callback)(func);  // 这会引起告警， 换成下面的方式就不会, i hate c++ !!!
    memcpy(&(cmd->proc), &(func), sizeof(void *));

    cmd->arg = (void *)arg; 
    conet::registry_cmd(server_name, cmd); 
    return 1; 
}



#define CONET_MACRO_CONCAT_IMPL(a, b) a##b
#define CONET_MACRO_CONCAT(a, b) CONET_MACRO_CONCAT_IMPL(a,b)

#define REGISTRY_RPC_PB_FUNC(server, method_name, func, arg) \
    static int CONET_MACRO_CONCAT(g_rpc_pb_registry_cmd_, __LINE__) = conet::registry_rpc_pb_cmd(server, method_name, func, arg)


}
#endif /* end of include guard */ 
