/*
 * =====================================================================================
 *
 *       Filename:  http_server.cpp
 *
 *    Description:  
 *
 *        Version:  1.0
 *        Created:  2014年07月26日 23时58分37秒
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
#include <errno.h>
#include <string.h>
#include <unistd.h>
#include <signal.h>
#include "svrkit/http_server.h"
#include "thirdparty/gflags/gflags.h"
#include "base/ip_list.h"
#include "base/plog.h"
#include "base/module.h"
#include "base/cpu_affinity.h"
#include "core/coroutine.h"

DEFINE_string(server_addr, "127.0.0.1:8080", "server address");
DEFINE_int32(server_stop_wait_seconds, 2, "server stop wait seconds");
DEFINE_int32(thread_num, 1, "server thread num");
DEFINE_string(cpu_set, "", "cpu affinity set");

using namespace conet;



static int g_server_stop_flag = 0;

int set_server_stop()
{
    g_server_stop_flag = 1;
    return 0;
}

int get_server_stop_flag()
{
    return g_server_stop_flag;
}

static void sig_exit(int sig)
{
    set_server_stop();
}




    struct Task
    {
    public:
        pthread_t tid;
        int exit_finsished;
        std::vector<ip_port_t> ip_list, http_ip_list;
        std::string http_address;

        tcp_server_t server;
        int http_listen_fd;
        int rpc_listen_fd;
        int cpu_id;
        uint64_t cnt;
        http_server_t g_server;
        std::string msg;

        Task()
        {
            exit_finsished = 0;
            http_listen_fd = -1;
            rpc_listen_fd = -1;
            cpu_id = -1;
            cnt = 0;
            msg = "hello\r\n";
            g_server.enable_keepalive=1;
            g_server.registry_cmd("/hello", &Task::proc_hello, this);
        }

        static int proc_server_exit(void *arg)
        {
            Task *self = (Task *)(arg);
            self->exit_finsished = 1;
            return 0;
        }

        static int proc_hello(void *arg, http_ctx_t *ctx, http_request_t *req, http_response_t *resp) {
            Task *task = ((Task*)(arg));
            //resp->body = task->msg;
            resp->data = task->msg.data();
            resp->data_len = task->msg.size();
            task->cnt++;
            return 0;
        }


        static void *proc(void *arg)
        {
            //pthread_setcanceltype(PTHREAD_CANCEL_DISABLE, NULL);
            conet::init_conet_env();
            int ret = 0;
            Task *self = (Task *)arg;
            if (self->cpu_id >= 0)
            {
                conet::set_cur_thread_cpu_affinity(self->cpu_id);
                self->server.cpu_id = self->cpu_id;
            }

            ret = self->server.init(self->ip_list[0].ip.c_str(), self->ip_list[0].port);
            if (ret)
            {
                PLOG_ERROR("init server faile!", (ret));
                return NULL;
            }
            self->g_server.init(&self->server);
            self->g_server.start();
            while (!self->exit_finsished)
            {
                conet::dispatch();
            }
            self->server.stop();
            self->g_server.stop();
            while (!self->g_server.has_stoped()) {
                conet::dispatch();
            }
            return NULL;
        }
    };


int main(int argc, char * argv[])
{
    InitAllModule(argc, argv);

    tcp_server_t tcp_server;
    std::vector<ip_port_t> ip_list;
    parse_ip_list(FLAGS_server_addr, &ip_list);
    if (ip_list.empty()) {
        fprintf(stderr, "server_addr:%s, format error!", FLAGS_server_addr.c_str());
        return 1;
    }

    signal(SIGINT, sig_exit);
    signal(SIGPIPE, SIG_IGN);

    std::vector<int> cpu_set;
    parse_affinity(FLAGS_cpu_set.c_str(), &cpu_set);

    int num = FLAGS_thread_num;
    Task *tasks = new Task[num];
    for (int i = 0; i < num; ++i)
    {
        tasks[i].ip_list = ip_list;
        if (!cpu_set.empty())
        {
            tasks[i].cpu_id = cpu_set[i % cpu_set.size()];
        }
        pthread_create(&tasks[i].tid, NULL, &Task::proc, tasks + i);
    }

    CO_RUN((tasks, num), {
        uint64_t prev_cnt = 0;
        while (!get_server_stop_flag()) {
            uint64_t cur_cnt = 0;
            for (int i = 0; i < num; ++i)
            {
                cur_cnt += tasks[i].cnt;
            }
            int cnt = cur_cnt - prev_cnt;
            prev_cnt = cur_cnt;
            if (cnt > 0)
                PLOG_INFO("qps:", cnt);
            sleep(1);
        }
    });

    int exit_finished = 0;
    while (likely((exit_finished < 2)))
    {
        if (unlikely(get_server_stop_flag() && exit_finished == 0))
        {
            exit_finished = 1;
            CO_RUN((exit_finished, tasks, num), {
                PLOG_INFO("server ready exit!");
                for (int i = 0; i < num; ++i)
                {
                    Task::proc_server_exit(tasks + i);
                }
                exit_finished = 2;
            });
        }
       conet::dispatch();
    }

    for (int i = 0; i < num; ++i)
    {
        pthread_join(tasks[i].tid, NULL);
    }

    delete[] tasks;

    return 0;
}
