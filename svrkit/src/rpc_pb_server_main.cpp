/*
 * =====================================================================================
 *
 *       Filename:  server_main.cpp
 *
 *    Description:  
 *
 *        Version:  1.0
 *        Created:  2014年09月15日 23时20分47秒
 *       Revision:  none
 *       Compiler:  gcc
 *
 *         Author:  piboye
 *   Organization:  
 *
 * =====================================================================================
 */

#include <stdlib.h>
#include <stdio.h>
#include <errno.h>
#include <string.h>
#include <unistd.h>
#include "rpc_pb_server.h"
#include "thirdparty/glog/logging.h"
#include "thirdparty/gflags/gflags.h"
#include "base/incl/ip_list.h"
#include "base/incl/delay_init.h"
#include "base/incl/net_tool.h"
#include "base/incl/cpu_affinity.h"
#include <linux/netdevice.h>

#include <signal.h>
#include <malloc.h>

DEFINE_string(http_server_address, "", "default use server address");
DEFINE_string(server_address, "0.0.0.0:12314", "default server address");

DEFINE_string(server_name, "", "server name");
DEFINE_bool(async_server, false, "async server");
DEFINE_int32(server_stop_wait_seconds, 2, "server stop wait seconds");
DEFINE_int32(thread_num, 1, "server thread num");

DEFINE_string(cpu_set, "", "cpu affinity set");


namespace conet
{
typedef void server_fini_func_t(void);
std::vector<server_fini_func_t *> g_server_fini_funcs;

int registry_server_fini_func(server_fini_func_t *func)
{
    g_server_fini_funcs.push_back(func); 
    return 1;
}

std::string g_rpc_server_name;

}

#define REG_SERVER_FININSH(func) \
    static int CONET_MACRO_CONCAT(g_registry_fini_, __LINE__) = conet::registry_server_fini_func(func);


using namespace conet;

static int g_exit_flag = 0;

static
void sig_exit(int sig)
{
   g_exit_flag=1; 
}


struct Task
{
    public:    
        pthread_t tid;
        int exit_finsished;
        std::vector<ip_port_t> ip_list, http_ip_list;
        std::string http_address;

        conet::rpc_pb_server_t server;    
        int http_listen_fd;
        int rpc_listen_fd;
        int cpu_id;

        Task()
        {
            exit_finsished = 0;
            http_listen_fd = -1;
            rpc_listen_fd = -1;
            cpu_id = -1;
        }

        static int proc_server_exit(void *arg)
        {
            Task *self = (Task *)(arg);
            int ret = 0;
            ret = conet::stop_server(&self->server, FLAGS_server_stop_wait_seconds*1000);
            self->exit_finsished = 1;
            return 0;
        }

        static void *proc(void *arg)
        {
            int ret = 0;

            Task *self = (Task *)arg;
            if (self->cpu_id >=0) {
                set_cur_thread_cpu_affinity(self->cpu_id);
            }

            if (self->http_ip_list.empty()) {
                ret = init_server(&self->server, g_rpc_server_name.c_str(), 
                        self->ip_list[0].ip.c_str(), self->ip_list[0].port);
            } else {
                ret = init_server(&self->server, g_rpc_server_name.c_str(), 
                        self->ip_list[0].ip.c_str(), self->ip_list[0].port, true, 
                        self->http_ip_list[0].ip.c_str(), self->http_ip_list[0].port);
            }

            if (ret) {
                fprintf(stderr, "listen to %s\n, failed, ret:%d\n", FLAGS_server_address.c_str(), ret);
                return 0;
            }

            fprintf(stdout, "listen to %s, http_listen:%s, success\n", FLAGS_server_address.c_str(), self->http_address.c_str());


            if (FLAGS_async_server) {
                self->server.async_flag = 1;
            }

            if (self->http_listen_fd >=0) {
                self->server.http_server->server->listen_fd = self->http_listen_fd;
            }

            if (self->rpc_listen_fd >=0) {
                self->server.server->listen_fd = self->rpc_listen_fd;
            }

            start_server(&self->server);

            coroutine_t *exit_co = NULL;
            while (!self->exit_finsished) {
                if (g_exit_flag && exit_co == NULL) {
                    exit_co = conet::alloc_coroutine(proc_server_exit, self);
                    conet::resume(exit_co);
                }
                conet::dispatch();
            }

            if (exit_co) {
                free_coroutine(exit_co);
            }
            return NULL;
        }
};

int main(int argc, char * argv[])
{
    int ret = 0;
    signal(SIGINT, sig_exit);

    mallopt(M_MMAP_THRESHOLD, 1024*1024); // 1MB，防止频繁mmap 
    mallopt(M_TRIM_THRESHOLD, 8*1024*1024); // 8MB，防止频繁brk 

    ret = google::ParseCommandLineFlags(&argc, &argv, false); 
    google::InitGoogleLogging(argv[0]);

    std::vector<int> cpu_set;
    parse_affinity(FLAGS_cpu_set.c_str(), &cpu_set);

    {
        // delay init
        delay_init::call_all_level();
        LOG(INFO)<<"delay init total:"<<delay_init::total_cnt
                <<" success:"<<delay_init::success_cnt
                <<", failed:"<<delay_init::failed_cnt;

        if(delay_init::failed_cnt>0)
        {
            LOG(ERROR)<<"delay init failed, failed num:"<<delay_init::failed_cnt;
            return 1;
        }
    }

    std::vector<ip_port_t> ip_list;
    parse_ip_list(FLAGS_server_address, &ip_list);
    if (ip_list.empty()) {
        fprintf(stderr, "server_addr:%s, format error!", FLAGS_server_address.c_str());
        return 1;
    }

    std::string http_address;
    std::vector<ip_port_t> http_ip_list;
    if (FLAGS_http_server_address.size() > 0) {
        http_address = FLAGS_http_server_address;
        parse_ip_list(http_address, &http_ip_list);
    } else {
        http_address = FLAGS_server_address;
    }

    if (g_rpc_server_name.empty()) {
        g_rpc_server_name = FLAGS_server_name;
        if (g_rpc_server_name.empty()) {
            g_rpc_server_name = conet::get_rpc_server_name_default();
            if (g_rpc_server_name.empty()) {
                LOG(ERROR)<<"please set server_name!";
                return 1;
            }
        }
    }


    if (FLAGS_thread_num <= 1) {
        Task task;
        task.ip_list = ip_list;
        task.http_ip_list = http_ip_list;
        task.http_address = http_address;
        if (!cpu_set.empty()) {
            task.cpu_id = cpu_set[0];
        }
        task.proc(&task);
    } else {

#if HAVE_SO_REUSEPORT
        int num = FLAGS_thread_num;
        Task *tasks = new Task[num];
        for (int i=0; i< num; ++i)
        {
            tasks[i].http_address = http_address;
            tasks[i].http_ip_list = http_ip_list;
            tasks[i].ip_list = ip_list;
            if (!cpu_set.empty()) {
                tasks[i].cpu_id = cpu_set[i%cpu_set.size()];
            }
            pthread_create(&tasks[i].tid, NULL, &Task::proc, tasks+i);
        }
#else

        int rpc_listen_fd = conet::create_tcp_socket(ip_list[0].port, ip_list[0].ip.c_str(), true);
        int http_listen_fd = rpc_listen_fd;
        if (!http_ip_list.empty()) {
            http_listen_fd = conet::create_tcp_socket(http_ip_list[0].port, http_ip_list[0].ip.c_str(), true);
        }

        int num = FLAGS_thread_num;
        Task *tasks = new Task[num];
        for (int i=0; i< num; ++i)
        {
            if (!cpu_set.empty()) {
                tasks[i].cpu_id = cpu_set[i%cpu_set.size()];
            }
            tasks[i].http_address = http_address;
            tasks[i].http_ip_list = http_ip_list;
            tasks[i].ip_list = ip_list;

            tasks[i].rpc_listen_fd = dup(rpc_listen_fd);
            if (http_listen_fd != rpc_listen_fd) {
                tasks[i].http_listen_fd = dup(http_listen_fd);
            }

            pthread_create(&tasks[i].tid, NULL, &Task::proc, tasks+i);
        }
#endif

        for (int i=0; i< num; ++i)
        {
            pthread_join(tasks[i].tid, NULL);
        }

        delete[] tasks;
    }
    



    for(size_t i=0, len = g_server_fini_funcs.size(); i<len; ++i) 
    {
        conet::server_fini_func_t *func = conet::g_server_fini_funcs[i];
        if (func) {
            func();
        }
    }

    return 0;
}

