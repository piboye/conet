/*
 * =====================================================================================
 *
 *       Filename:  rpc_pb_server_base_impl.h
 *
 *    Description:  
 *
 *        Version:  1.0
 *        Created:  2014年11月02日 22时06分29秒
 *       Revision:  none
 *       Compiler:  gcc
 *
 *         Author:  piboye
 *   Organization:  
 *
 * =====================================================================================
 */

#ifndef __RPC_PB_SERVER_BASE_IMPL_H__
#define __RPC_PB_SERVER_BASE_IMPL_H__

#include "rpc_pb_server_base.h"
#include "http_server.h"
#include "base/incl/str_map.h"
#include "base/incl/int_map.h"

namespace conet
{

struct rpc_pb_server_base_t;

int rpc_pb_http_call_cb(void *arg, http_ctx_t *ctx, http_request_t * req, http_response_t *resp);
int rpc_pb_call_cb(rpc_pb_cmd_t *self, rpc_pb_ctx_t *ctx, std::string *req, std::string *rsp, std::string *errmsg);
        
struct rpc_pb_server_base_t
{
    StrMap cmd_maps;
    IntMap cmdid_maps;

    rpc_pb_server_base_t();
    rpc_pb_cmd_t * get_rpc_pb_cmd(char const *name, size_t name_len);

    int get_global_server_cmd();

    int registry_http_rpc_default_api(http_server_t *http_server, std::string const &base_path);
    int registry_rpc_cmd_http_api(http_server_t *http_server, std::string const & method_name, rpc_pb_cmd_t *cmd, std::string const & base_path);
    int registry_all_rpc_http_api(http_server_t *http_server, std::string const &base_path);
};


}
#endif /* end of include guard */