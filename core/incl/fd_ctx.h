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
 *         Author:  YOUR NAME (),
 *   Organization:
 *
 * =====================================================================================
 */

#ifndef __FD_CTX_H_INC__
#define __FD_CTX_H_INC__
#include "list.h"
#include <sys/time.h>
#include <sys/socket.h>
#include <sys/un.h>
#include <netinet/in.h>
namespace conet
{

struct fd_ctx_t
{
    enum {
        SOCKET_FD_TYPE=1,
        DISK_FD_TYPE=2,
        TIMER_FD_TYPE=2,
    };
    int type;
    int fd;
    int use_cnt;
    int user_flag;
    struct sockaddr_in dest; //maybe sockaddr_un;
    int domain; //AF_LOCAL , AF_INET

    int rcv_timeout;
    int snd_timeout;
    uint32_t wait_events;
    list_head poll_wait_queue;
};


fd_ctx_t * alloc_fd_ctx(int fd, int type=1);
fd_ctx_t * get_fd_ctx(int fd, int type =1);

int free_fd_ctx(int fd);

void incr_ref_fd_ctx(fd_ctx_t *obj);
void decr_ref_fd_ctx(fd_ctx_t *obj);

}

#endif /* end of include guard */
