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

namespace conet
{

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

    std::vector<char> out_buf;

    ret = send_pb_obj(fd, req_base, &out_buf, timeout);

    if (ret <=0) {
        LOG(ERROR)<<"[rpc_pb_client] send request failed, [ret:"<<ret<<"][errno:"<<errno<<"]"<<strerror(errno)<<"]";
        return -4;
    }
    PacketStream stream;
    stream.init(fd, 1024*1024);
    char * data = NULL;
    int packet_len = 0;

    ret = stream.read_packet(&data, &packet_len, timeout);

    if (ret <=0) {
        LOG(ERROR)<<"[rpc_pb_client] recv respose failed, [ret:"<<ret<<"][errno:"<<errno<<"][strerr:"<<strerror(errno)<<"]";
        return -5;
    }

    if (!resp_base.ParseFromArray(data, packet_len)) {
        LOG(ERROR)<<"[rpc_pb_client] parse respose failed";
        return -6;
    }
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


