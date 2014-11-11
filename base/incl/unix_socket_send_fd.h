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


namespace conet
{

    struct UnixSocketSendFd
    {

        int uinx_socks[2];

        UnixSocketSendFd()
        {
            uinx_socks[0]=-1;
            uinx_socks[1]=-1;
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
        
        int send_fd(int fd)
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

        int recv_fd(int *fd)
        {
            int sock = uinx_socks[1];

            struct msghdr message;
            struct iovec iov[1];
            struct cmsghdr *control_message = NULL;
            char ctrl_buf[CMSG_SPACE(sizeof(int))];
            char data[1];
            int res;

            memset(&message, 0, sizeof(struct msghdr));
            memset(ctrl_buf, 0, CMSG_SPACE(sizeof(int)));

            /* For the dummy data */
            iov[0].iov_base = data;
            iov[0].iov_len = sizeof(data);

            message.msg_name = NULL;
            message.msg_namelen = 0;
            message.msg_control = ctrl_buf;
            message.msg_controllen = CMSG_SPACE(sizeof(int));
            message.msg_iov = iov;
            message.msg_iovlen = 1;

            if((res = recvmsg(socket, &message, MSG_DONTWAIT)) <= 0)
                return res;

            for(control_message = CMSG_FIRSTHDR(&message);
                    control_message != NULL;
                    control_message = CMSG_NXTHDR(&message,
                        control_message))
            {
                if( (control_message->cmsg_level == SOL_SOCKET) &&
                        (control_message->cmsg_type == SCM_RIGHTS) )
                {
                    *fd = *((int *) CMSG_DATA(control_message));
                    return 0;
                }
            }

            return -1;
        }


    };

}


#endif /* end of include guard */
