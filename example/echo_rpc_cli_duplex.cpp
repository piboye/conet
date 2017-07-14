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
#include "svrkit/rpc_pb_client_duplex.h"
#include "thirdparty/gflags/gflags.h"
#include "base/plog.h"
#include "core/conet_all.h"

#include "base/net_tool.h"

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
                PLOG_ERROR("ret:", ret);
            if (retcode)
            PLOG_ERROR("ret_code:",retcode," resposne:",resp.DebugString());
            continue;
        }
        //if (i %100 == 99) usleep(10000);
        //LOG(ERROR)<<"success";
    }
    ++g_finish_task_num;
    return 0;
}

class Main: public Coroutine
{
    ::task_t *tasks;
    public:
        int run()
        {
            exit = 0;
            conet::RpcPbClientDuplex *client = new conet::RpcPbClientDuplex(); 

            int ret = 0;
            ret = client->init(FLAGS_server_addr.c_str());
            if (ret) {
                PLOG_ERROR("init server address ",FLAGS_server_addr," failed!, ret:",ret);
                exit = 1;
                return -1;
            }

            tasks = new ::task_t[FLAGS_task_num];
            for (int i=0; i<FLAGS_task_num; ++i) {
                tasks[i].co = conet::alloc_coroutine(proc_send, tasks+i);
                tasks[i].client = client;
                conet::resume(tasks[i].co);
            }

            while (g_finish_task_num < FLAGS_task_num) {
                sleep(1);
            }
            exit = 1;
            return 0;
        }
        int exit;
};

int main(int argc, char * argv[])
{
    gflags::ParseCommandLineFlags(&argc, &argv, false); 

    if (prepare_data(FLAGS_data_file.c_str())) {
        PLOG_ERROR("read data failed!");
        return 1;
    }

    Main c1;
    c1.resume();
    while (!c1.exit)
    {
        conet::dispatch();
    }

    return 0;
}

