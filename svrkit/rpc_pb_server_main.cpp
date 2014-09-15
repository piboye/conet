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
#include "svrkit/rpc_pb_server.h"
#include "thirdparty/glog/logging.h"
#include "thirdparty/gflags/gflags.h"
#include "svrkit/ip_list.h"
#include "delay_init.h"

DEFINE_string(http_server_address, "", "default use server address");
DEFINE_string(server_address, "0.0.0.0:12314", "default server address");

DEFINE_string(server_name, "", "server name");

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

int main(int argc, char * argv[])
{
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


    start_server(&g_server);

    while (conet::get_epoll_pend_task_num() >0) {
        conet::dispatch();
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

