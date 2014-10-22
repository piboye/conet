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
#include <signal.h>
#include <malloc.h>

DEFINE_string(http_server_address, "", "default use server address");
DEFINE_string(server_address, "0.0.0.0:12314", "default server address");

DEFINE_string(server_name, "", "server name");
DEFINE_bool(async_server, false, "async server");
DEFINE_int32(server_stop_wait_seconds, 2, "server stop wait seconds");

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

static rpc_pb_server_t g_server;

static int g_exit_flag = 0;
static int g_exit_finsished = 0;

static
void sig_exit(int sig)
{
   g_exit_flag=1; 
}

static int proc_server_exit(void *)
{
    int ret = 0;
    ret = conet::stop_server(&g_server, FLAGS_server_stop_wait_seconds*1000);
    g_exit_finsished = 1;
    return 0;
}

int main(int argc, char * argv[])
{
    mallopt(M_MMAP_THRESHOLD, 1024*1024); // 1MB，防止频繁mmap 
    mallopt(M_TRIM_THRESHOLD, 8*1024*1024); // 8MB，防止频繁brk 

    google::ParseCommandLineFlags(&argc, &argv, false); 
    google::InitGoogleLogging(argv[0]);

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

    int ret = 0;
    std::vector<ip_port_t> ip_list;
    parse_ip_list(FLAGS_server_address, &ip_list);
    if (ip_list.empty()) {
        fprintf(stderr, "server_addr:%s, format error!", FLAGS_server_address.c_str());
        return 1;
    }

    std::string http_address;
    std::vector<ip_port_t> http_ip_list;
    if (FLAGS_http_server_address.size() > 0) {
        parse_ip_list(FLAGS_http_server_address, &http_ip_list);
        http_address = FLAGS_http_server_address;
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

    if (http_ip_list.empty()) {
        ret = init_server(&g_server, g_rpc_server_name.c_str(), ip_list[0].ip.c_str(), ip_list[0].port);
    } else {
        ret = init_server(&g_server, g_rpc_server_name.c_str(), ip_list[0].ip.c_str(), ip_list[0].port, true, 
                          http_ip_list[0].ip.c_str(), http_ip_list[0].port);
    }

    if (ret) {
        fprintf(stderr, "listen to %s\n, failed, ret:%d\n", FLAGS_server_address.c_str(), ret);
        return 1;
    }

    fprintf(stdout, "listen to %s, http_listen:%s, success\n", FLAGS_server_address.c_str(), http_address.c_str());


    signal(SIGINT, sig_exit);

    if (FLAGS_async_server) {
        g_server.async_flag = 1;
    }
    start_server(&g_server);

    coroutine_t *exit_co = NULL;
    while (!g_exit_finsished) {
        if (g_exit_flag && exit_co == NULL) {
            exit_co = conet::alloc_coroutine(proc_server_exit, NULL);
            conet::resume(exit_co);
        }
        conet::dispatch();
    }

    if (exit_co) {
        free_coroutine(exit_co);
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

