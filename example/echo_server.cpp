/*
 * =====================================================================================
 *
 *       Filename:  echo_server.cpp
 *
 *    Description:
 *
 *        Version:  1.0
 *        Created:  2014年05月11日 07时50分16秒
 *       Revision:  none
 *       Compiler:  gcc
 *
 *         Author:  YOUR NAME (),
 *   Organization:
 *
 * =====================================================================================
 */
#include <stdlib.h>
#include <stdio.h>
#include <errno.h>
#include <string.h>
#include <signal.h>
#include <unistd.h>
#include "svrkit/tcp_server.h"
#include "thirdparty/gflags/gflags.h"
#include "base/plog.h"
#include "base/cpu_affinity.h"
#include "base/module.h"
#include "base/ip_list.h"
#include "base/net_tool.h"
#include "core/coroutine.h"

using namespace conet;
DEFINE_string(server_address, "0.0.0.0:12314", "default server address");
DEFINE_int32(server_stop_wait_seconds, 2, "server stop wait seconds");
DEFINE_int32(thread_num, 1, "server thread num");
DEFINE_string(cpu_set, "", "cpu affinity set");

uint64_t g_cnt = 0;

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

namespace
{

    int proc_echo(void *arg, conn_info_t *conn);

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

        Task()
        {
            exit_finsished = 0;
            http_listen_fd = -1;
            rpc_listen_fd = -1;
            cpu_id = -1;
            cnt = 0;
        }

        static int proc_server_exit(void *arg)
        {
            Task *self = (Task *)(arg);
            self->server.stop();
            self->exit_finsished = 1;
            return 0;
        }

        static void *proc(void *arg)
        {
            conet::init_conet_env();
            int ret = 0;
            Task *self = (Task *)arg;
            if (self->cpu_id >= 0)
            {
                set_cur_thread_cpu_affinity(self->cpu_id);
                self->server.cpu_id = self->cpu_id;
            }

            ret = self->server.init(self->ip_list[0].ip.c_str(), self->ip_list[0].port);
            if (ret)
            {
                PLOG_ERROR("init server faile!", (ret));
                return NULL;
            }
            self->server.set_conn_cb(proc_echo, NULL);
            self->server.start();
            while (!self->exit_finsished)
            {
                conet::dispatch();
            }
            return NULL;
        }
    };

    int proc_echo(void *arg, conn_info_t *conn)
    {
        conet::enable_sys_hook();
        tcp_server_t *svr = (tcp_server_t *)conn->server;
        Task *task = NULL;
        task = container_of(svr, Task, server);
        int size = svr->conf.max_packet_size;
        char *buff = GC_ALLOC_ARRAY(char, size);
        int ret = 0;
        int fd = conn->fd;
        do {
            do {
                ret = conet::poll_recv(fd, buff, size, 1000);
            } while (ret == -1 && errno == EINTR);
            if (ret <= 0)
                break;

            do {
                ret = send(fd, buff, ret, 0);
            } while (ret == -1 && errno == EINTR);
            if (ret <= 0)
                break;
            task->cnt++;
        } while (1);
        return 0;
    }
}

DEFINE_string(server_addr, "0.0.0.0:12314", "server address");

int main(int argc, char *argv[])
{
    // conet::init_conet_global_env();

    InitAllModule(argc, argv);

    std::vector<ip_port_t> ip_list;
    parse_ip_list(FLAGS_server_addr, &ip_list);
    if (ip_list.empty())
    {
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
        while (!get_server_stop_flag())
        {
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
