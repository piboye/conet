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
 *         Author:  piboye
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
#include "base/plog.h"
#include "core/conet_all.h"

#include "base/net_tool.h"
#include "base/defer.h"
#include "base/module.h"


DEFINE_string(server_addr, "127.0.0.1:12314", "server address");
DEFINE_int32(task_num, 10, "concurrent task num");
DEFINE_int32(send_num, 100000, "send num per task");
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
    uint64_t cmd_id = 3;
    std::string errmsg;
    int send_num = FLAGS_send_num;
    int fd = task->lb->get();
    for (int i=0; i<send_num; ++i) 
    {
        int retcode=0;
        if (fd < 0) break;
        ret = conet::rpc_pb_call(fd, cmd_id, (google::protobuf::Message *)NULL, (google::protobuf::Message *)NULL, &retcode, &errmsg);
        if (ret) {
            PLOG_ERROR((ret));
            close(fd);
            fd = task->lb->get();
            continue;
        }

        if (retcode) {
            close(fd);
            fd = task->lb->get();
            PLOG_ERROR((retcode, errmsg));
        }
    }
    if (fd >= 0) close(fd);
    ++g_finish_task_num;
    return 0;
}

task_t *tasks = NULL;
int main(int argc, char * argv[])
{
    InitAllModule(argc, argv);

    conet::IpListLB lb; 
    lb.init(FLAGS_server_addr);

    conet::init_conet_global_env();
    CONET_DEFER({
        conet::free_conet_global_env();
    });

    tasks = new task_t[FLAGS_task_num];
    for (int i=0; i<FLAGS_task_num; ++i) {
        tasks[i].co = conet::alloc_coroutine(proc_send, tasks+i);
        tasks[i].lb = &lb;
        resume(tasks[i].co);
        conet::dispatch();
    }

    while (g_finish_task_num < FLAGS_task_num) {
        conet::dispatch();
    }

    return 0;
}

