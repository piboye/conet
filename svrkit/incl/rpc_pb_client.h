/*
 * =====================================================================================
 *
 *       Filename:  rpc_pb_client.h
 *
 *    Description:  
 *
 *        Version:  1.0
 *        Created:  2014年07月08日 16时26分30秒
 *       Revision:  none
 *       Compiler:  gcc
 *
 *         Author:  piboyeliu
 *   Organization:  
 *
 * =====================================================================================
 */

#ifndef RPC_PB_CLIENT_H_INC_
#define RPC_PB_CLIENT_H_INC_
#include <string>
#include <list>
#include <queue>

#include "svrkit/rpc_base_pb.pb.h"
#include "base/incl/net_tool.h"
#include "base/incl/list.h"
#include "base/incl/ip_list.h"
#include "load_balance.h"

namespace conet
{
//client 
int rpc_pb_call_impl(int fd,
        std::string const &cmd_name,
        std::string const &req, std::string *resp, int *retcode, std::string *errmsg, int timeout);

int rpc_pb_call_impl(int fd,
        uint64_t cmd_id,
        std::string const &req, std::string *resp, int *retcode, std::string *errmsg, int timeout);

template <typename ReqT, typename RespT, typename CmdNameT>
int rpc_pb_call(int fd, 
        CmdNameT const & cmd_name,
        ReqT const *a_req, RespT *a_resp, int *retcode, std::string *errmsg=NULL, int timeout=1000)
{
    std::string req;
    if (!a_req->SerializeToString(&req)) {
        return -8;
    }
    std::string resp;
    int ret = 0;

    ret = rpc_pb_call_impl(fd , cmd_name, req, &resp, retcode, errmsg, timeout);

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


template <typename ReqT, typename RespT, typename CmdNameT>
int rpc_pb_call(char const *ip, int port, 
        CmdNameT const & cmd_name,
        ReqT const *a_req, RespT *a_resp,
        int *retcode, std::string *errmsg=NULL, int timeout=1000)
{
    int fd = 0;
    fd = connect_to(ip, port);
    if (fd <0) return -3;
    int ret = rpc_pb_call(fd, cmd_name, a_req, a_resp, retcode, errmsg, timeout);
    close(fd);
    return fd;
}



template <typename ReqT, typename RespT, typename LBT, typename CmdNameT>
int rpc_pb_call(LBT &lb,
        CmdNameT const & cmd_name,
        ReqT const *a_req, RespT *a_resp, 
        int *retcode, std::string *errmsg=NULL, int timeout=1000)
{
    int fd = 0;
    //std::string ip;
    //int port;
    fd = lb.get();
    if (fd <0) return -3;
    int ret =  rpc_pb_call(fd, cmd_name, a_req, a_resp, retcode, errmsg, timeout);
    lb.release(fd, ret);
    return ret;
}

}

#endif /* end of include guard */
