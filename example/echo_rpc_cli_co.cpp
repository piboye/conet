/*
 * =====================================================================================
 *
 *       Filename:  echo_rpc_cli_co.cpp
 *
 *    Description:  
 *
 *        Version:  1.0
 *        Created:  2014年07月08日 19时20分08秒
 *       Revision:  none
 *       Compiler:  gcc
 *
 *         Author:  piboyeliu
 *   Organization:  
 *
 * =====================================================================================
 */

#include <stdlib.h>
#include <stdlib.h>
#include <stdio.h>
#include "net_tool.h"
#include "example/echo_rpc.pb.h"
#include "svrkit/rpc_pb_client.h"
#include "thirdparty/gflags/gflags.h"
#include "conet_all.h"

DEFINE_string(server_addr, "127.0.0.1:12314", "server address");
DEFINE_int32(task_num, 10, "concurrent task num");
DEFINE_string(data_file, "1.txt", "send data file");

struct task_t
{
    conet::IpListLB *lb;
    conet::coroutine_t *co;
};

int proc_send(void *arg)
{
    conet::enable_sys_hook();
    task_t *task = (task_t *)(arg);

    int ret = 0;
    char *line= NULL;
    size_t len = 0;
    FILE *fp = fopen(FLAGS_data_file.c_str(), "r");
    if (!fp) {
        fprintf(stderr, "open file:%s failed!\n", FLAGS_data_file.c_str());
        return -1;
    }
    while( (ret = getline(&line, &len, fp)) >= 0) {
        EchoReq req;
        EchoResp resp;
        req.set_msg(std::string(line, ret));
        int retcode=0;
        ret = conet::rpc_pb_call(*task->lb, "echo", "echo", &req, &resp, &retcode);
        if (ret || retcode)
            printf("ret:%d, ret_code:%d, response:%s\n", ret, retcode, resp.msg().c_str());
    }
    return 0;
}


int main(int argc, char * argv[])
{
    google::ParseCommandLineFlags(&argc, &argv, false); 

    conet::IpListLB lb; 
    lb.init(FLAGS_server_addr);

    task_t * tasks = new task_t[FLAGS_task_num];
    for (int i=0; i<FLAGS_task_num; ++i) {
        tasks[i].co = conet::alloc_coroutine(proc_send, tasks+i);
        tasks[i].lb = &lb;
        resume(tasks[i].co);
    }

    while (conet::get_epoll_pend_task_num() >0) {
        conet::dispatch();
    }

    return 0;
}

