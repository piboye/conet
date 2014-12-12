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
#include "base/incl/ip_list.h"
#include "base/incl/net_tool.h"

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

}

