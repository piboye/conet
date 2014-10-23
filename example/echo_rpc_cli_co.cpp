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
#include "example/echo_rpc.pb.h"
#include "svrkit/incl/rpc_pb_client.h"
#include "thirdparty/gflags/gflags.h"
#include "thirdparty/glog/logging.h"
#include "conet_all.h"

#include "base/incl/net_tool.h"

#ifdef USE_VALGRIND
#include <valgrind/valgrind.h>
#endif

DEFINE_string(server_addr, "127.0.0.1:12314", "server address");
DEFINE_int32(task_num, 10, "concurrent task num");
DEFINE_string(data_file, "1.txt", "send data file");

struct task_t
{
    conet::IpListLB *lb;
    conet::coroutine_t *co;
};
int g_finish_task_num=0;
int proc_send(void *arg)
{
    conet::enable_sys_hook();
    conet::enable_pthread_hook();
    task_t *task = (task_t *)(arg);

    int ret = 0;
    char *line= NULL;
    size_t len = 0;
    FILE *fp = fopen(FLAGS_data_file.c_str(), "r");
    if (!fp) {
        fprintf(stderr, "open file:%s failed!\n", FLAGS_data_file.c_str());
        ++g_finish_task_num;
        return -1;
    }
    while( (ret = getline(&line, &len, fp)) >= 0) {
    //line = "hello";
    //for (int i=0; i<100000; ++i) {
        EchoReq req;

        EchoResp resp;
        req.set_msg(std::string(line));
        int retcode=0;
        ret = conet::rpc_pb_call(*task->lb, "echo", "echo", &req, &resp, &retcode);
        if (ret || retcode) {
            LOG(ERROR)<<"ret:"<<ret;
            continue;
        }

        if (retcode)
            LOG(ERROR)<<"ret_code:"<<retcode<<" resposne:"<<resp.DebugString();;
    }
    ++g_finish_task_num;
    return 0;
}

task_t *tasks = NULL;
int main(int argc, char * argv[])
{
    google::ParseCommandLineFlags(&argc, &argv, false); 
    google::InitGoogleLogging(argv[0]);

    conet::IpListLB lb; 
    lb.init(FLAGS_server_addr);

    tasks = new task_t[FLAGS_task_num];
    for (int i=0; i<FLAGS_task_num; ++i) {
        tasks[i].co = conet::alloc_coroutine(proc_send, tasks+i);
        tasks[i].lb = &lb;
        resume(tasks[i].co);
    }

    while (g_finish_task_num < FLAGS_task_num) {
        conet::dispatch();
    }

#ifdef USE_VALGRIND
    VALGRIND_STACK_DEREGISTER(0);
#endif
    return 0;
}

