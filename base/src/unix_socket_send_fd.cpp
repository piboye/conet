/*
 * =====================================================================================
 *
 *       Filename:  unix_socket_send_fd.cpp
 *
 *    Description:  
 *
 *        Version:  1.0
 *        Created:  2014年11月11日 09时47分41秒
 *       Revision:  none
 *       Compiler:  gcc
 *
 *         Author:  piboye
 *   Organization:  
 *
 * =====================================================================================
 */
#include <stdlib.h>
#include "unix_socket_send_fd.h"
namespace conet
{
        int UnixSocketSendFd::send_fd(int fd)
        {
            int sock = unix_socks[0];

            struct msghdr hdr;
            struct iovec data;

            char cmsgbuf[CMSG_SPACE(sizeof(int))];

            char dummy = '*';
            data.iov_base = &dummy;
            data.iov_len = sizeof(dummy);

            memset(&hdr, 0, sizeof(hdr));
            hdr.msg_name = NULL;
            hdr.msg_namelen = 0;
            hdr.msg_iov = &data;
            hdr.msg_iovlen = 1;
            hdr.msg_flags = 0;

            hdr.msg_control = cmsgbuf;
            hdr.msg_controllen = CMSG_LEN(sizeof(int));

            struct cmsghdr* cmsg = CMSG_FIRSTHDR(&hdr);
            cmsg->cmsg_len   = CMSG_LEN(sizeof(int));
            cmsg->cmsg_level = SOL_SOCKET;
            cmsg->cmsg_type  = SCM_RIGHTS;

            *(int*)CMSG_DATA(cmsg) = fd;

            int ret = sendmsg(sock, &hdr, MSG_DONTWAIT);
            if (ret <=0) {
                return ret;
            }

            return 0;
        }

        int UnixSocketSendFd::recv_fd(std::vector<int> *fds)
        {
            int sock = unix_socks[1];

            int ret = 0;

            struct msghdr msg;
            struct iovec iov[1];
            struct cmsghdr *cmsg = NULL;
            char ctrl_buf[CMSG_SPACE(sizeof(int))];
            char data[1];
            int res;

            memset(&msg, 0, sizeof(struct msghdr));
            memset(ctrl_buf, 0, CMSG_SPACE(sizeof(int)));

            /* For the dummy data */
            iov[0].iov_base = data;
            iov[0].iov_len = sizeof(data);

            msg.msg_name = NULL;
            msg.msg_namelen = 0;
            msg.msg_control = ctrl_buf;
            msg.msg_controllen = CMSG_SPACE(sizeof(int));
            msg.msg_iov = iov;
            msg.msg_iovlen = 1;


            if((ret = recvmsg(sock, &msg, MSG_DONTWAIT)) <= 0)
                return ret;

            if ((msg.msg_flags & MSG_TRUNC) || (msg.msg_flags & MSG_CTRUNC))
            {
                return -1;
            }

           int fd=-1; 
           for (cmsg = CMSG_FIRSTHDR(&msg); cmsg != NULL; cmsg = CMSG_NXTHDR(&msg, cmsg)) 
           {
                   if (cmsg->cmsg_len == CMSG_LEN(sizeof(int)) &&
                       cmsg->cmsg_level == SOL_SOCKET &&
                       cmsg->cmsg_type == SCM_RIGHTS) 
                   {
                           fd = *(int *)CMSG_DATA(cmsg);
                           fds->push_back(fd);
                   }
           }
            
           return 0;
        }
}
