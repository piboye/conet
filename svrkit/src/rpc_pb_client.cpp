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
#include "../../base/incl/ptr_cast.h"
#include <poll.h>
#include "../../core/incl/coroutine.h"
#include "cmd_base.h"

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
    int max_size = 1*4096;
    PacketStream *ps = new PacketStream(max_size);
    ps->is_http = 0;
    return ps;
}

static __thread obj_pool_t *g_packet_stream_pool = NULL;

static __thread uint64_t g_rpc_client_seq_id = 0;

obj_pool_t * get_rpc_pb_client_packet_stream_pool()
{
    if (unlikely(NULL == g_packet_stream_pool)) {
        obj_pool_t *pool = new obj_pool_t();
        pool->set_alloc_obj_func(ptr_cast<obj_pool_t::alloc_func_t>(&alloc_packet_stream), NULL);
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
        conet::cmd_base_t *req_base,
        google::protobuf::Message const *req, ref_str_t * rsp, int *retcode, 
        std::string *errmsg, int timeout, 
        obj_pool_t *ps_pool,
        PacketStream **ps2
        )
{

    int ret = 0;
    req_base->type = conet_rpc_pb::CmdBase::REQUEST_TYPE;
    req_base->seq_id = ++g_rpc_client_seq_id;

    PacketStream *stream = (PacketStream *) ps_pool->alloc();
    ret = send_cmd_base(fd, stream, req_base, req, timeout);
    ps_pool->release(stream);

    if (ret <=0) {
        LOG(ERROR)<<"[rpc_pb_client] send request failed, [fd:"<<fd<<"][ret:"<<ret<<"][errno:"<<errno<<"]"<<strerror(errno)<<"]";
        return -4;
    }

    char * data = NULL;
    int packet_len = 0;

    struct pollfd pf = { fd : fd, events: ( POLLIN | POLLERR | POLLHUP ) };
    ret =  poll( &pf, 1, timeout );
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

    *ps2 = stream;

    ret = stream->read_packet(&data, &packet_len, timeout, 1);
    

    if (ret <=0) {
        LOG(ERROR)<<"[rpc_pb_client] recv response failed, [fd:"<<fd<<"][ret:"<<ret<<"][errno:"<<errno<<"]"<<strerror(errno)<<"]";
        return -5;
    }

    cmd_base_t rsp_base;
    rsp_base.init();
    ret =  rsp_base.parse(data, packet_len);

    if (ret) 
    {
        LOG(ERROR)<<"[rpc_pb_client] parse response failed, [fd:"<<fd<<"][ret:"<<ret<<"]";
        return -6;
    }

    *retcode = rsp_base.ret;
    if (*retcode) {
        if (errmsg && rsp_base.errmsg.len >0)
        {
            ref_str_to(&rsp_base.errmsg, errmsg);
        }
        return 0;
    }

    if (rsp) {
        *rsp = rsp_base.body;
    }
    return 0;
}

}


