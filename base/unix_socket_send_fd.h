/*
 * =====================================================================================
 *
 *       Filename:  unix_socket_send_fd.h
 *
 *    Description:  
 *
 *        Version:  1.0
 *        Created:  2014年11月11日 07时48分56秒
 *       Revision:  none
 *       Compiler:  gcc
 *
 *         Author:  piboye
 *   Organization:  
 *
 * =====================================================================================
 */

#ifndef __CONET_UNIX_SOCKET_SEND_FD_H__
#define __CONET_UNIX_SOCKET_SEND_FD_H__

#include <stdio.h>
#include <sys/types.h>
#include <sys/socket.h>
#include <vector>
#include "net_tool.h"


namespace conet
{

    struct UnixSocketSendFd
    {

        int unix_socks[2];

        UnixSocketSendFd()
        {
            unix_socks[0]=-1;
            unix_socks[1]=-1;
        }

        ~UnixSocketSendFd()
        {
            close(unix_socks[0]);
            close(unix_socks[1]);
        }

        int init() 
        {
            int ret = 0;
            ret = socketpair(AF_UNIX, SOCK_STREAM, 0, unix_socks);
            if (0 == ret) {
                set_none_block(unix_socks[0]);
                set_none_block(unix_socks[1]);
            }
            return ret;
        }


        inline
        int get_send_handle()
        {
            return unix_socks[0];
        }

        inline
        int get_recv_handle() 
        {
            return unix_socks[1];
        }
        

        int send_fd(int fd);
        int recv_fd(std::vector<int> *fds);

    };

}

#endif /* end of include guard */
