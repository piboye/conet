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
#include "thirdparty/glog/logging.h"
#include "core/conet_all.h"

#include "base/net_tool.h"
#include "base/defer.h"


DEFINE_string(server_addr, "127.0.0.1:12314", "server address");
DEFINE_int32(task_num, 10, "concurrent task num");
DEFINE_string(data_file, "1.txt", "send data file");
DEFINE_string(cmd_name, "", "echo or echo2");
DEFINE_uint64(cmd_id, 3, " cmd id 1, 2, 3");

std::vector<std::string *> g_data;
int prepare_data(char const *file)
{
    char *line= NULL;
    char rbuff[1024];
    FILE *fp = fopen(file, "r");
    if (!fp) {
        fprintf(stderr, "open file:%s failed!", file); 
        return -1;
    }
    int ret = 0;
    
    while(1) {
        size_t size = 1000;
        ret = getline(&line, &size, fp);
        if (ret <= 0) break;
        g_data.push_back(new std::string(rbuff, ret));
    }

    return 0;
}

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
    EchoReq req;
    EchoResp resp;
    std::string method = FLAGS_cmd_name;
    uint64_t cmd_id = FLAGS_cmd_id;
    std::string errmsg;
    if (!method.empty())
    {
        cmd_id = 0;
    }
    for (int i=0, len = g_data.size(); i<len; ++i) {
        req.set_msg(*g_data[i]);
        int retcode=0;
        if (cmd_id) {
            ret = conet::rpc_pb_call(*task->lb, cmd_id, &req, &resp, &retcode, &errmsg);
        } else {
            ret = conet::rpc_pb_call(*task->lb, method, &req, &resp, &retcode, &errmsg);
        }
        if (ret) {
            LOG(ERROR)<<"ret:"<<ret;
            continue;
        }

        if (retcode)
            LOG(ERROR)<<"ret_code:"<<retcode<<" errmsg "<<errmsg<<" resposne:"<<resp.DebugString();;
    }
    ++g_finish_task_num;
    return 0;
}

task_t *tasks = NULL;
int main(int argc, char * argv[])
{
    gflags::ParseCommandLineFlags(&argc, &argv, false); 
    google::InitGoogleLogging(argv[0]);

    if (prepare_data(FLAGS_data_file.c_str())) {
        LOG(ERROR)<<"read data failed!";
        return 1;
    }

    conet::init_conet_global_env();
    CONET_DEFER({
        conet::free_conet_global_env();
    });

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

    for (int i=0, len = g_data.size(); i<len; ++i) 
    {
        delete g_data[i];
    }
    g_data.clear();

    return 0;
}

