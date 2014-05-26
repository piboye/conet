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
#include "net_tool.h"
#include "example/echo_rpc.pb.h"
#include "server/rpc_pb_server.h"

int main(int argc, char const* argv[])
{
    if (argc < 3) {
        fprintf(stderr, "usage:%s ip port\n", argv[0]);
        return 0;
    }
    char const * ip = argv[1];
    int  port = atoi(argv[2]);

    int ret = 0;
    char *line= NULL;
    size_t len = 0;
    while( (ret = getline(&line, &len, stdin)) >= 0) {
        if (ret == 0) continue;
        EchoReq req;
        EchoResp resp;
        req.set_msg(std::string(line, ret));
        int ret_code = 0; 
        ret_code = conet::rpc_pb_call(ip, port, "echo", "echo", &req, &resp, NULL);
        printf("ret_code:%d, response:%s\n", ret_code, resp.msg().c_str());
    }
    free(line);
    return 0;
}

