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
#include "server_common.h"

#include "thirdparty/glog/logging.h"
#include "thirdparty/gflags/gflags.h"
#include "base/ip_list.h"
#include "base/net_tool.h"

namespace conet
{
    DEFINE_bool(thread_mode, false, "multithread");

    int is_thread_mode()
    {
        return FLAGS_thread_mode;
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

    int create_listen_fd(ip_port_t const &ip_port)
    {

            int rpc_listen_fd = conet::create_tcp_socket(ip_port.port, ip_port.ip.c_str(), true);
            if (rpc_listen_fd<0) {
                LOG(ERROR)<<"listen to ["<<ip_port.ip<<":"<<ip_port.port<<"failed!";
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
}

