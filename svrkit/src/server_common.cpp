/*
 * =====================================================================================
 *
 *       Filename:  server_common.cpp
 *
 *    Description:  
 *
 *        Version:  1.0
 *        Created:  2014年12月12日 07时44分38秒
 *       Revision:  none
 *       Compiler:  gcc
 *
 *         Author:  piboye
 *   Organization:  
 *
 * =====================================================================================
 */

#include <stdlib.h>
#include <string>
#include <map>
#include "server_common.h"
#include <pthread.h>
#include <functional>
#include <thread>
#include <signal.h>
#include "core/conet_all.h"

#include "thirdparty/gflags/gflags.h"
#include "base/ip_list.h"
#include "base/net_tool.h"
#include "base/plog.h"
#include "base/module.h"

namespace conet
{

    int is_thread_mode()
    {
        return 1;
    }

    int get_listen_fd(char const *ip, int port, int listen_fd)
    {
        if (conet::can_reuse_port() || listen_fd <0) {
            int rpc_listen_fd = conet::create_tcp_socket(port, ip, true);
            return rpc_listen_fd;
        } else {
            if (is_thread_mode()) { 
                return dup(listen_fd);
            } else {
                return listen_fd;
            }
        }
    }

    static std::map<ip_port_t, int> g_listen_fd_pool;
    pthread_mutex_t g_listen_fd_pool_mutex=PTHREAD_MUTEX_INITIALIZER;

    int get_listen_fd_from_pool(char const *ip, int port)
    {
        int fd = -1;
        pthread_mutex_lock(&g_listen_fd_pool_mutex);
        ip_port_t addr;
        addr.ip = ip;
        addr.port = port;
        typeof(g_listen_fd_pool.begin()) it = g_listen_fd_pool.find(addr);
        if (it == g_listen_fd_pool.end()) {
           fd = create_listen_fd(addr); 
           if (fd >= 0) {
                g_listen_fd_pool.insert(std::make_pair(addr, fd));
           }
        } else {
           fd =  dup(it->second); 
        }
        pthread_mutex_unlock(&g_listen_fd_pool_mutex);
        return fd;
    }

    int create_listen_fd(ip_port_t const &ip_port)
    {

            int rpc_listen_fd = conet::create_tcp_socket(ip_port.port, ip_port.ip.c_str(), true);
            if (rpc_listen_fd<0) {
                PLOG_ERROR("listen to [",ip_port.ip,":",ip_port.port,"failed!");
                return -1;
            }

            listen(rpc_listen_fd,  1000);
            set_none_block(rpc_listen_fd);
            return rpc_listen_fd;
    }

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

    typedef void server_fini_func_t(void);
    std::vector<server_fini_func_t *> g_server_fini_funcs;

    int registry_server_fini_func(server_fini_func_t *func)
    {
        g_server_fini_funcs.push_back(func); 
        return 1;
    }

    int call_server_fini_func()
    {
        for(size_t i=0, len = g_server_fini_funcs.size(); i<len; ++i) 
        {
            conet::server_fini_func_t *func = conet::g_server_fini_funcs[i];
            if (func) {
                func();
            }
        }
        return 0;
    }


    std::thread && co_thread_run(std::function<void(void)> op) {
        auto fn = [](std::function<void(void)> f1) {
            conet::init_conet_env();
            conet::coroutine_t *co = NewCo((f1), {f1();}); \
            conet::set_auto_delete(co); \
            conet::resume(co); \
            while(!get_server_stop_flag() && conet::get_epoll_pend_task_num()>0) {
                conet::dispatch();
            }
        };
        return std::move(std::thread(fn, op));
    }

    static void sig_exit(int sig)
    {
        set_server_stop();
    }


    DEFINE_MODULE(server_stop){
        signal(SIGINT, sig_exit);
        signal(SIGPIPE, SIG_IGN);
        return 0;
    }
}

