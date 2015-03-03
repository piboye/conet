/*
 * =====================================================================================
 *
 *       Filename:  echo_cli.cpp
 *
 *    Description:
 *
 *        Version:  1.0
 *        Created:  2014年05月11日 08时16分30秒
 *       Revision:  none
 *       Compiler:  gcc
 *
 *         Author:  YOUR NAME (),
 *   Organization:
 *
 * =====================================================================================
 */
#include <stdlib.h>
#include <stdlib.h>
#include <stdio.h>
#include "base/net_tool.h"
#include "example/echo_rpc.pb.h"
#include "svrkit/rpc_pb_client.h"
#include "thirdparty/gflags/gflags.h"
#include "base/net_tool.h"
#include "base/ip_list.h"

DEFINE_string(server_addr, "127.0.0.1:12314", "server address");

using namespace conet;

int main(int argc, char * argv[])
{
    gflags::ParseCommandLineFlags(&argc, &argv, false); 
    google::InitGoogleLogging(argv[0]);

    std::vector<ip_port_t> ip_list;
    parse_ip_list(FLAGS_server_addr,  &ip_list);

    if (ip_list.size() <=0) {
        LOG(ERROR)<<"error server addr:"<<FLAGS_server_addr;
        return 1;
    }

    int ret = 0;
    int fd = create_udp_socket();
    struct sockaddr_in addr;
    set_addr(&addr, ip_list[0].ip.c_str(), ip_list[0].port);
    ret = connect(fd, (struct sockaddr*)&addr,sizeof(addr));
    char *line= NULL;
    size_t len = 0;
    while( (ret = getline(&line, &len, stdin)) >= 0) {
        if (ret == 0) continue;
        EchoReq req;
        EchoResp resp;
        req.set_msg(std::string(line, ret));
        int retcode = 0; 
        ret = conet::rpc_pb_udp_call(fd, "echo", &req, &resp, &retcode, NULL);
        printf("ret:%d, retcode:%d, response:%s\n", ret, retcode, resp.msg().c_str());
    }
    close(fd);
    free(line);
    return 0;
}

