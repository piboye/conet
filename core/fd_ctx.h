/*
 * =====================================================================================
 *
 *       Filename:  fd_ctx.h
 *
 *    Description:
 *
 *        Version:  1.0
 *        Created:  2014年05月06日 15时53分39秒
 *       Revision:  none
 *       Compiler:  gcc
 *
 *         Author:  piboye
 *   Organization:
 *
 * =====================================================================================
 */

#ifndef __FD_CTX_H_INC__
#define __FD_CTX_H_INC__

#include <sys/time.h>
#include <sys/socket.h>
#include <sys/un.h>
#include <netinet/in.h>

#include "base/list.h"

namespace conet
{

struct fd_ctx_t
{
    enum {
        SOCKET_FD_TYPE=1,
        DISK_FD_TYPE=2,
        TIMER_FD_TYPE=3,
    };
    struct sockaddr_in dest; //maybe sockaddr_un;
    int type;
    int fd;
    int user_flag;
    int closed;

    int rcv_timeout;
    int snd_timeout;
    int domain;
};

int init_fd_ctx_env();


fd_ctx_t * alloc_fd_ctx(int fd, int type=1);

fd_ctx_t * alloc_fd_ctx2(int fd, int type, int has_nonblocked);

fd_ctx_t * get_fd_ctx(int fd, int type =1);

int free_fd_ctx(int fd);

}

#endif /* end of include guard */
