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

#include "rpc_pb_client.h"

namespace conet
{

int rpc_pb_call_impl(int fd,
        std::string const &server_name,
        std::string const &cmd_name,
        std::string const &req, std::string *resp, std::string *errmsg)
{
    if (fd <0) return -3;
    int ret = 0;

    conet_rpc_pb::CmdBase req_base, resp_base;
    req_base.set_server_name(server_name);
    req_base.set_cmd_name(cmd_name);
    req_base.set_seq_id(1);
    req_base.set_type(conet_rpc_pb::CmdBase::REQUEST_TYPE);
    req_base.set_body(req);

    ret = send_data_pack(fd, req_base.SerializeAsString());
    if (ret <=0) {
        //close(fd);
        return -4;
    }
    PacketStream stream;
    stream.init(fd, 1024*1024);
    char * data = NULL;
    int packet_len = 0;

    ret = stream.read_packet(&data, &packet_len);

    //close(fd);
    if (ret <=0) {
        return -5;
    }
    if (!resp_base.ParseFromArray(data, packet_len)) {
        return -6;
    }
    ret = resp_base.ret();
    if (ret) {
        if (errmsg) {
            *errmsg = resp_base.errmsg();
        }
        return ret;
    }

    if (resp) {
        *resp = resp_base.body();
    }
    return ret;
}

}
