/*
 * =====================================================================================
 *
 *       Filename:  rpc_pb_client.cpp
 *
 *    Description:  
 *
 *        Version:  1.0
 *        Created:  2014年07月08日 16时25分11秒
 *       Revision:  none
 *       Compiler:  gcc
 *
 *         Author:  piboyeliu
 *   Organization:  
 *
 * =====================================================================================
 */
#include <stdlib.h>
#include <string>
#include <string.h>

#include <errno.h>
#include "rpc_pb_client.h"
#include "glog/logging.h"
#include "../../base/incl/obj_pool.h"
#include "../../base/incl/tls.h"
#include "../../base/incl/fn_ptr_cast.h"
#include <poll.h>
#include "../../core/incl/coroutine.h"

namespace conet
{

static 
void free_packet_stream(void *arg, void *ps)
{
    PacketStream *ps2 = (PacketStream *)(ps);
    delete ps2;
}

static
PacketStream *alloc_packet_stream()
{
    int max_size = 1024*1024;
    PacketStream *ps = new PacketStream(max_size);
    ps->is_http = 0;
    return ps;
}

static __thread obj_pool_t *g_packet_stream_pool = NULL;

static 
obj_pool_t * get_packet_stream_pool()
{
    if (unlikely(NULL == g_packet_stream_pool)) {
        obj_pool_t *pool = new obj_pool_t();
        pool->set_alloc_obj_func(fn_ptr_cast<obj_pool_t::alloc_func_t>(&alloc_packet_stream), NULL);
        pool->set_free_obj_func(&free_packet_stream, NULL);
        if (NULL == g_packet_stream_pool) {
            g_packet_stream_pool = pool;
        }else {
            delete pool;
        }
    }
    return g_packet_stream_pool;
}


int rpc_pb_call_impl(int fd,
        std::string const &cmd_name,
        std::string const &req, std::string *resp, int *retcode, std::string *errmsg, int timeout)
{
    if (fd <0) {
        LOG(ERROR)<<"[rpc_pb_client] errr fd [fd:"<<fd<<"][errno:"<<errno<<"]"<<strerror(errno)<<"]";
        return -3;
    }
    int ret = 0;

    conet_rpc_pb::CmdBase req_base, resp_base;
    req_base.set_cmd_name(cmd_name);
    req_base.set_seq_id(1);
    req_base.set_type(conet_rpc_pb::CmdBase::REQUEST_TYPE);
    req_base.set_body(req);

    obj_pool_t * ps_pool = get_packet_stream_pool();

    PacketStream *stream = (PacketStream *) ps_pool->alloc();
    stream->init(fd);

    ret = send_pb_obj(fd, req_base, stream->buff, stream->max_size, timeout);

    ps_pool->release(stream);

    if (ret <=0) {
        LOG(ERROR)<<"[rpc_pb_client] send request failed, [fd:"<<fd<<"][ret:"<<ret<<"][errno:"<<errno<<"]"<<strerror(errno)<<"]";
        return -4;
    }
    char * data = NULL;
    int packet_len = 0;

    struct pollfd pf = { fd : fd, events: ( POLLIN | POLLERR | POLLHUP ) };
    ret =  co_poll( &pf, 1, timeout );
    if (ret == 0) {
        // timeout;
        return -2;
    }
    if (ret <0) {
        return -1;
    }
    if (pf.revents & POLLERR) {
        return -1;
    }
    if (!(pf.revents &POLLIN))
    {
        LOG(ERROR)<<"poll write failed, [events:"<<pf.revents<<"]";
        return -1;
    }

    stream = (PacketStream *) ps_pool->alloc();
    stream->init(fd);

    ret = stream->read_packet(&data, &packet_len, timeout, 1);

    if (ret <=0) {
        ps_pool->release(stream);
        LOG(ERROR)<<"[rpc_pb_client] recv response failed, [fd:"<<fd<<"][ret:"<<ret<<"][errno:"<<errno<<"]"<<strerror(errno)<<"]";
        return -5;
    }

    if (!resp_base.ParseFromArray(data, packet_len)) {

        ps_pool->release(stream);
        LOG(ERROR)<<"[rpc_pb_client] parse response failed, [fd:"<<fd<<"][ret:"<<ret<<"]";
        return -6;
    }

    ps_pool->release(stream);
    *retcode = resp_base.ret();
    if (*retcode) {
        if (errmsg && (resp_base.has_errmsg())) {
            *errmsg = resp_base.errmsg();
        }
        return 0;
    }

    if (resp) {
        *resp = resp_base.body();
    }
    return 0;
}


int rpc_pb_call_impl(int fd,
        uint64_t cmd_id,
        std::string const &req, std::string *resp, int *retcode, std::string *errmsg, int timeout)
{
    if (fd <0) {
        LOG(ERROR)<<"[rpc_pb_client] errr fd [fd:"<<fd<<"][errno:"<<errno<<"]"<<strerror(errno)<<"]";
        return -3;
    }
    int ret = 0;

    conet_rpc_pb::CmdBase req_base, resp_base;
    req_base.set_cmd_id(cmd_id);
    req_base.set_seq_id(1);
    req_base.set_type(conet_rpc_pb::CmdBase::REQUEST_TYPE);
    req_base.set_body(req);

    obj_pool_t * ps_pool = get_packet_stream_pool();

    PacketStream *stream = (PacketStream *) ps_pool->alloc();
    stream->init(fd);
    ret = send_pb_obj(fd, req_base, stream->buff, stream->max_size, timeout);
    ps_pool->release(stream);

    if (ret <=0) {
        LOG(ERROR)<<"[rpc_pb_client] send request failed, [fd:"<<fd<<"][ret:"<<ret<<"][errno:"<<errno<<"]"<<strerror(errno)<<"]";
        return -4;
    }
    char * data = NULL;
    int packet_len = 0;

    struct pollfd pf = { fd : fd, events: ( POLLIN | POLLERR | POLLHUP ) };
    ret =  co_poll( &pf, 1, timeout );
    if (ret == 0) {
        // timeout;
        return -2;
    }
    if (ret <0) {
        return -1;
    }
    if (pf.revents & POLLERR) {
        return -1;
    }
    if (!(pf.revents &POLLIN))
    {
        LOG(ERROR)<<"poll write failed, [events:"<<pf.revents<<"]";
        return -1;
    }

    stream = (PacketStream *) ps_pool->alloc();
    stream->init(fd);

    ret = stream->read_packet(&data, &packet_len, timeout, 1);

    if (ret <=0) {
        ps_pool->release(stream);
        LOG(ERROR)<<"[rpc_pb_client] recv response failed, [fd:"<<fd<<"][ret:"<<ret<<"][errno:"<<errno<<"]"<<strerror(errno)<<"]";
        return -5;
    }

    if (!resp_base.ParseFromArray(data, packet_len)) {
        ps_pool->release(stream);
        LOG(ERROR)<<"[rpc_pb_client] parse response failed, [fd:"<<fd<<"][ret:"<<ret<<"]";
        return -6;
    }

    ps_pool->release(stream);

    *retcode = resp_base.ret();
    if (*retcode) {
        if (errmsg && (resp_base.has_errmsg())) {
            *errmsg = resp_base.errmsg();
        }
        return 0;
    }

    if (resp) {
        *resp = resp_base.body();
    }
    return 0;
}

}


