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
DEFINE_int32(send_num, 100000, "send num per task");

using namespace conet;

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
    std::string errmsg;
    int send_num = FLAGS_send_num;

    for (int i=0; i<send_num; ++i) 
    {
        int retcode=0;
        ret = task->client->rpc_call(3,  NULL, NULL, &retcode, NULL, 1000);
        if (ret || retcode) {
            PLOG_ERROR((ret, retcode));
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

    Main c1;
    c1.resume();
    while (!c1.exit)
    {
        conet::dispatch();
    }
    return 0;
}

