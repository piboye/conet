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

struct fd_ctx_mgr_t
{
    fd_ctx_t **fds;
    int size;
};


int init_fd_ctx_env();


fd_ctx_t * alloc_fd_ctx(int fd, int type=1);

fd_ctx_t * alloc_fd_ctx2(int fd, int type, int has_nonblocked);

extern fd_ctx_mgr_t *g_fd_ctx_mgr;

inline int get_default_fd_ctx_size();
inline fd_ctx_mgr_t *create_fd_ctx_mgr(int size);
inline fd_ctx_mgr_t * get_fd_ctx_mgr()
{
	if (!g_fd_ctx_mgr) {
		g_fd_ctx_mgr = create_fd_ctx_mgr(get_default_fd_ctx_size());
	}
	return g_fd_ctx_mgr;
}
fd_ctx_t *get_fd_ctx(int fd, int type=1);

int free_fd_ctx(int fd);


void fd_ctx_expand(fd_ctx_mgr_t *mgr, int need_size);

inline
fd_ctx_t *get_fd_ctx(int fd, int type)
{
    if (fd <0) return NULL;

    fd_ctx_mgr_t *mgr = g_fd_ctx_mgr;

    if (fd >= mgr->size ) {
        fd_ctx_expand(mgr, fd+1);
    }

    fd_ctx_t *ctx =  mgr->fds[fd];
    if (NULL == ctx) {
        /*
        struct stat sb;
        int ret = fstat(fd, &sb);
        if (ret) return NULL;
        if (S_ISSOCK(sb.st_mode)) {
            return alloc_fd_ctx(fd, fd_ctx_t::SOCKET_FD_TYPE);
        }
        if (S_ISFIFO(sb.st_mode)) {
            return alloc_fd_ctx(fd, fd_ctx_t::SOCKET_FD_TYPE);
        }
        if (S_ISCHR(sb.st_mode)) {
            return alloc_fd_ctx(fd, fd_ctx_t::SOCKET_FD_TYPE);
        }
        // because FILE read buffer, can't hook
        if (S_ISREG(sb.st_mode)) {
            return alloc_fd_ctx(fd, fd_ctx_t::DISK_FD_TYPE);
        }
        */
        return NULL;
    }

    if (type == 0) return ctx;

    if (ctx->type == type) {
        return ctx;
    }
    return NULL;
}


}

#endif /* end of include guard */
