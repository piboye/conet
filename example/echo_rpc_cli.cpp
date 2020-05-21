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
#include "example/echo_rpc.pb.h"
#include "svrkit/rpc_pb_client.h"
#include "thirdparty/gflags/gflags.h"
#include "base/net_tool.h"
#include "base/module.h"

DEFINE_string(server_addr, "127.0.0.1:12314", "server address");

int main(int argc, char * argv[])
{

    InitAllModule(argc, argv);

    conet::IpListLB lb; 
    lb.init(FLAGS_server_addr);
    int ret = 0;
    char *line= NULL;
    size_t len = 0;
    while( (ret = getline(&line, &len, stdin)) >= 0) {
        if (ret == 0) continue;
        EchoReq req;
        EchoResp resp;
        req.set_msg(std::string(line, ret));
        int retcode = 0; 
        ret = conet::rpc_pb_call(lb, "echo", &req, &resp, &retcode, NULL);
        printf("ret:%d, retcode:%d, response:%s\n", ret, retcode, resp.msg().c_str());
    }
    free(line);
    return 0;
}

