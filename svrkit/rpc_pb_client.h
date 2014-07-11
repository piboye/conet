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
#include "svrkit/rpc_base_pb.pb.h"
#include "net_tool.h"
#include <list>
#include <queue>
#include "core/incl/list.h"
#include "ip_list.h"
#include "load_balance.h"

namespace conet
{
//client 
int rpc_pb_call_impl(int fd,
        std::string const &server_name,
        std::string const &cmd_name,
        std::string const &req, std::string *resp, std::string *errmsg);


template <typename ReqT, typename RespT>
int rpc_pb_call(int fd, 
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

    ret = rpc_pb_call_impl(fd , server_name, cmd_name, req, &resp, errmsg);

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

template <typename ReqT, typename RespT>
int rpc_pb_call(char const *ip, int port, 
        std::string const &server_name,
        std::string const &cmd_name,
        ReqT const *a_req, RespT *a_resp, std::string *errmsg=NULL)
{
    int fd = 0;
    fd = connect_to(ip, port);
    if (fd <0) return -3;
    int ret = rpc_pb_call(fd, server_name, cmd_name, a_req, a_resp, errmsg);
    close(fd);
    return fd;
}



template <typename ReqT, typename RespT, typename LBT>
int rpc_pb_call(LBT &lb,
        std::string const &server_name,
        std::string const &cmd_name,
        ReqT const *a_req, RespT *a_resp, std::string *errmsg=NULL)
{
    int fd = 0;
    std::string ip;
    int port;
    fd = lb.get(&ip, &port);
    if (fd <0) return -3;
    int ret =  rpc_pb_call(fd, server_name, cmd_name, a_req, a_resp, errmsg);
    if (ret == 0) {
        lb.release(ip.c_str(), port, fd, 0);
    } else {
        lb.release(ip.c_str(), port, fd, 1);
    }
    return ret;
}

}

#endif /* end of include guard */
