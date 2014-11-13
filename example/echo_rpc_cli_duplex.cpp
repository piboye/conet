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
#include "svrkit/incl/rpc_pb_client_duplex.h"
#include "thirdparty/gflags/gflags.h"
#include "thirdparty/glog/logging.h"
#include "conet_all.h"

#include "base/incl/net_tool.h"

DEFINE_string(server_addr, "127.0.0.1:12314", "server address");
DEFINE_int32(task_num, 10, "concurrent task num");
DEFINE_string(data_file, "1.txt", "send data file");

using namespace conet;

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
    RpcPbClientDuplex * client;
    conet::coroutine_t *co;
};

int g_finish_task_num=0;
int proc_send(void *arg)
{
    conet::enable_sys_hook();
    conet::enable_pthread_hook();
    ::task_t *task = (::task_t *)(arg);

    int ret = 0;

    EchoReq req;
    EchoResp resp;
    for (int i=0, len = g_data.size(); i<len; ++i) {
        req.set_msg(*g_data[i]);
        int retcode=0;
        ret = task->client->rpc_call(2, &req, &resp, &retcode, NULL, 1000);
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

::task_t *tasks = NULL;
int main(int argc, char * argv[])
{
    google::ParseCommandLineFlags(&argc, &argv, false); 
    google::InitGoogleLogging(argv[0]);

    if (prepare_data(FLAGS_data_file.c_str())) {
        LOG(ERROR)<<"read data failed!";
        return 1;
    }

    conet::RpcPbClientDuplex client; 

    int ret = 0;
    ret = client.init(FLAGS_server_addr.c_str());
    
    tasks = new ::task_t[FLAGS_task_num];
    for (int i=0; i<FLAGS_task_num; ++i) {
        tasks[i].co = conet::alloc_coroutine(proc_send, tasks+i);
        tasks[i].client = &client;
        resume(tasks[i].co);
    }

    while (g_finish_task_num < FLAGS_task_num) {
        conet::dispatch();
    }

    return 0;
}

