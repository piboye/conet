/*
 * =====================================================================================
 *
 *       Filename:  fd_ctx.cpp
 *
 *    Description:
 *
 *        Version:  1.0
 *        Created:  2014年05月06日 15时55分54秒
 *       Revision:  none
 *       Compiler:  gcc
 *
 *         Author:  piboyeliu
 *   Organization:
 *
 * =====================================================================================
 */
#include <stdlib.h>
#include "fd_ctx.h"
#include <unistd.h>
#include <fcntl.h>
#include <dlfcn.h>
#include <assert.h>
#include <sys/stat.h>

#include "log.h"
#include "coroutine_impl.h"
#include "coroutine.h"

#include <sys/resource.h>
#include "hook_helper.h"
#include "gflags/gflags.h"

#include "../../base/tls.h"

DEFINE_int32(fd_ctx_size, 0, "default use fd size");

HOOK_DECLARE(int, fcntl, (int, int, ...));

namespace conet
{

struct fd_ctx_mgr_t
{
    fd_ctx_t **fds;
    int size;
};

fd_ctx_mgr_t *create_fd_ctx_mgr(int size)
{
    fd_ctx_mgr_t *mgr = new fd_ctx_mgr_t();
    mgr->fds = new fd_ctx_t *[size+1];
    memset(mgr->fds, 0, (size+1) * sizeof(fd_ctx_t *));
    mgr->size = size;
    return mgr;
}

void free_fd_ctx_mgr(fd_ctx_mgr_t *mgr)
{
    for (int i = 0; i <= mgr->size; i++) {
        fd_ctx_t *ctx = mgr->fds[i];
        delete ctx;
    }
    delete [] mgr->fds;
    delete mgr;
}

__thread fd_ctx_mgr_t * g_fd_ctx_mgr = NULL;

static inline
void expand(fd_ctx_mgr_t *mgr, int need_size)
{
    int size = mgr->size;
    while (size <= need_size) {
        size +=10000;
    }

    fd_ctx_t ** fds=  new fd_ctx_t * [size+1];
    memset(fds, 0, (size+1) *sizeof(void *));
    memcpy(fds, mgr->fds, (mgr->size+1) * sizeof(void *) ); 
    mgr->fds = fds;
    mgr->size = size;
}

static 
int get_default_fd_ctx_size()
{
    if (FLAGS_fd_ctx_size > 0) {
        return FLAGS_fd_ctx_size;
    } else {
        struct rlimit rl;
        int ret = 0;
        ret = getrlimit( RLIMIT_NOFILE, &rl);
        if (ret) {
            return 100000;
        } 
        return rl.rlim_max;
    }
}

CONET_DEF_TLS_VAR_HELP(g_fd_ctx_mgr, 
        ({
            create_fd_ctx_mgr(get_default_fd_ctx_size());
         }),

        ({
            free_fd_ctx_mgr(self);
         })
);


fd_ctx_t *get_fd_ctx(int fd, int type)
{
    if (fd <0) return NULL;

    fd_ctx_mgr_t *mgr = TLS_GET(g_fd_ctx_mgr);

    if (fd >  mgr->size) {
        assert("!too many fd");
        exit(1);
    }
    fd_ctx_t *ctx =   mgr->fds[fd];
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

fd_ctx_t * alloc_fd_ctx2(int fd, int type, int has_nonblocked)
{
    if (fd <0) {
        assert(!"fd <0");
        exit(1);
        return NULL;
    }

    fd_ctx_mgr_t *mgr = TLS_GET(g_fd_ctx_mgr);

    if (fd >= mgr->size ) {
        expand(mgr, fd);
    }

    fd_ctx_t * d = mgr->fds[fd];

    if ( NULL == d ) {
        d =  new fd_ctx_t;
        mgr->fds[fd] = d;
    }

    d->type = type;
    d->fd = fd;
    d->rcv_timeout = 1000;
    d->snd_timeout = 1000;
    d->domain = 0;
    int flags = 0;
    flags = _(fcntl)(fd, F_GETFL, 0);
    //default is block 
    d->user_flag = 0;

    if (!has_nonblocked) {
        // 设置 none block, 方便hook 系统调用
        // user_flag 只保存用户设置的标志。
        _(fcntl)(fd, F_SETFL, flags | O_NONBLOCK);
    }

    return mgr->fds[fd];
}

fd_ctx_t *alloc_fd_ctx(int fd, int type)
{
    return alloc_fd_ctx2(fd, type, false);
}

int free_fd_ctx(int fd)
{
    if (fd <0) return -1;

    fd_ctx_mgr_t *mgr = TLS_GET(g_fd_ctx_mgr);

    if (fd > mgr->size ) {
        return -2;
    }

    fd_ctx_t * d = mgr->fds[fd];

    if (NULL == d) return -2;

    mgr->fds[fd] = NULL;

    delete d;
    return 0;
}

}
