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
