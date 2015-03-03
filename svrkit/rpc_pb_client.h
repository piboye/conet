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

#include "cmd_base.h"
#include "svrkit/rpc_base_pb.pb.h"
#include "../base/net_tool.h"
#include "../base/list.h"
#include "../base/ip_list.h"
#include "load_balance.h"
#include "../../base/obj_pool.h"
#include "glog/logging.h"

namespace conet
{

//client 
obj_pool_t * get_rpc_pb_client_packet_stream_pool();

int rpc_pb_call_impl(int fd,
        conet::cmd_base_t *req_base,
        google::protobuf::Message const *req, ref_str_t * rsp, int *retcode, 
        std::string *errmsg, int timeout, 
        obj_pool_t *ps_pool,
        PacketStream **ps2
        );

int rpc_pb_call_udp_impl( 
        int fd,
        conet::cmd_base_t *req_base,
        google::protobuf::Message const *req, ref_str_t * rsp, int *retcode, 
        std::string *errmsg, int timeout, 
        obj_pool_t *ps_pool,
        PacketStream **ps2
        );

template <typename ReqT, typename RespT>
inline
int rpc_pb_udp_call(int fd, 
        cmd_base_t *req_base,
        ReqT const *req, RespT *a_rsp, int *retcode, std::string *errmsg=NULL, int timeout=1000)
{
    if (fd <0) {
        LOG(ERROR)<<"[rpc_pb_client] errr fd [fd:"<<fd<<"]";
        return -3;
    }

    int ret = 0;

    ref_str_t rsp;
    
    obj_pool_t * ps_pool = get_rpc_pb_client_packet_stream_pool();
    PacketStream *ps = NULL;
    ret = rpc_pb_call_udp_impl(fd , req_base, req, &rsp,  retcode, errmsg, timeout, ps_pool,  &ps);
    if (ps) {
        ps_pool->release(ps);
    }

    if (ret) {
        return ret;
    }

    if (a_rsp && rsp.len >0) {
        if (!a_rsp->ParseFromArray(rsp.data, rsp.len)) {
            LOG(ERROR)<<"[rpc_pb_client] paser response msg failed!";
            return -7;
        }
    }
    return 0;
}

template <typename ReqT, typename RespT>
inline
int rpc_pb_call(int fd, 
        cmd_base_t *req_base,
        ReqT const *req, RespT *a_rsp, int *retcode, std::string *errmsg=NULL, int timeout=1000)
{
    if (fd <0) {
        LOG(ERROR)<<"[rpc_pb_client] errr fd [fd:"<<fd<<"]";
        return -3;
    }

    int ret = 0;

    ref_str_t rsp;
    
    obj_pool_t * ps_pool = get_rpc_pb_client_packet_stream_pool();
    PacketStream *ps = NULL;
    ret = rpc_pb_call_impl(fd , req_base, req, &rsp,  retcode, errmsg, timeout, ps_pool,  &ps);
    if (ps) {
        ps_pool->release(ps);
    }

    if (ret) {
        return ret;
    }

    if (a_rsp && rsp.len >0) {
        if (!a_rsp->ParseFromArray(rsp.data, rsp.len)) {
            LOG(ERROR)<<"[rpc_pb_client] paser response msg failed!";
            return -7;
        }
    }
    return 0;
}

template <typename ReqT, typename RespT>
inline
int rpc_pb_call(int fd, 
        uint64_t const & cmd_id,
        ReqT const *a_req, RespT *a_resp, int *retcode, std::string *errmsg=NULL, int timeout=1000)
{

    conet::cmd_base_t req_base;
    req_base.init();
    req_base.cmd_id = cmd_id;
    return rpc_pb_call(fd, &req_base, a_req, a_resp, retcode, errmsg, timeout);
}

template <typename ReqT, typename RespT>
inline
int rpc_pb_udp_call(int fd, 
        uint64_t const & cmd_id,
        ReqT const *a_req, RespT *a_resp, int *retcode, std::string *errmsg=NULL, int timeout=1000)
{

    conet::cmd_base_t req_base;
    req_base.init();
    req_base.cmd_id = cmd_id;
    return rpc_pb_udp_call(fd, &req_base, a_req, a_resp, retcode, errmsg, timeout);
}

template <typename ReqT, typename RespT>
inline
int rpc_pb_udp_call(int fd, 
        ref_str_t cmd_name,
        ReqT const *a_req, RespT *a_resp, int *retcode, std::string *errmsg=NULL, int timeout=1000)
{

    conet::cmd_base_t req_base;
    req_base.init();
    req_base.cmd_name = cmd_name;
    return rpc_pb_udp_call(fd, &req_base, a_req, a_resp, retcode, errmsg, timeout);
}

template <typename ReqT, typename RespT>
inline
int rpc_pb_call(int fd, 
        ref_str_t cmd_name,
        ReqT const *a_req, RespT *a_resp, int *retcode, std::string *errmsg=NULL, int timeout=1000)
{

    conet::cmd_base_t req_base;
    req_base.init();
    req_base.cmd_name = cmd_name;
    return rpc_pb_call(fd, &req_base, a_req, a_resp, retcode, errmsg, timeout);
}

template <typename ReqT, typename RespT>
inline
int rpc_pb_udp_call(int fd, 
        std::string const &cmd_name,
        ReqT const *a_req, RespT *a_resp, int *retcode, std::string *errmsg=NULL, int timeout=1000)
{

    ref_str_t cmd_name2;
    init_ref_str(&cmd_name2, cmd_name);
    return rpc_pb_udp_call(fd, cmd_name2, a_req, a_resp, retcode, errmsg, timeout);
}

template <typename ReqT, typename RespT>
inline
int rpc_pb_call(int fd, 
        std::string const &cmd_name,
        ReqT const *a_req, RespT *a_resp, int *retcode, std::string *errmsg=NULL, int timeout=1000)
{

    ref_str_t cmd_name2;
    init_ref_str(&cmd_name2, cmd_name);
    return rpc_pb_call(fd, cmd_name2, a_req, a_resp, retcode, errmsg, timeout);
}

template <typename ReqT, typename RespT>
inline
int rpc_pb_udp_call(int fd, 
        char const * cmd_name,
        ReqT const *a_req, RespT *a_resp, int *retcode, std::string *errmsg=NULL, int timeout=1000)
{

    ref_str_t cmd_name2;
    init_ref_str(&cmd_name2, cmd_name);
    return rpc_pb_udp_call(fd, cmd_name2, a_req, a_resp, retcode, errmsg, timeout);
}

template <typename ReqT, typename RespT>
inline
int rpc_pb_call(int fd, 
        char const * cmd_name,
        ReqT const *a_req, RespT *a_resp, int *retcode, std::string *errmsg=NULL, int timeout=1000)
{

    ref_str_t cmd_name2;
    init_ref_str(&cmd_name2, cmd_name);
    return rpc_pb_call(fd, cmd_name2, a_req, a_resp, retcode, errmsg, timeout);
}


template <typename ReqT, typename RespT, typename CmdNameT>
inline
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

template <typename ReqT, typename RespT, typename CmdNameT>
inline
int rpc_pb_udp_call(char const *ip, int port, 
        CmdNameT const & cmd_name,
        ReqT const *a_req, RespT *a_resp,
        int *retcode, std::string *errmsg=NULL, int timeout=1000)
{
    int fd = create_udp_socket();

    if (fd <0) return -3;
    struct sockaddr_in addr;
    set_addr(&addr, ip, port);
    int ret = 0;
    ret = connect(fd, (struct sockaddr*)&addr,sizeof(addr));
    if (ret) {
        *errmsg="connect failed!";
        return -4;
    }
    ret = rpc_pb_udp_call(fd, cmd_name, a_req, a_resp, retcode, errmsg, timeout);
    close(fd);
    return fd;
}



template <typename ReqT, typename RespT, typename LBT, typename CmdNameT>
inline
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

template <typename ReqT, typename RespT, typename LBT, typename CmdNameT>
inline
int rpc_pb_udp_call(LBT &lb,
        CmdNameT const & cmd_name,
        ReqT const *a_req, RespT *a_resp, 
        int *retcode, std::string *errmsg=NULL, int timeout=1000)
{
    int fd = 0;
    //std::string ip;
    //int port;
    fd = lb.get();
    if (fd <0) return -3;
    int ret =  rpc_pb_udp_call(fd, cmd_name, a_req, a_resp, retcode, errmsg, timeout);
    lb.release(fd, ret);
    return ret;
}

}

#endif /* end of include guard */
