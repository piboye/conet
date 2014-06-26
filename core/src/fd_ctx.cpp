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
#include "log.h"
#include "coroutine_impl.h"
#include "coroutine.h"
#include "tls.h"

#include "hook_helper.h"

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
    fd_ctx_mgr_t *mgr = (fd_ctx_mgr_t *)malloc(sizeof(fd_ctx_mgr_t));
    mgr->fds = (fd_ctx_t**)malloc(sizeof(void *) *(size+1));
    for (int i=0; i<=size; ++i) {
        mgr->fds[i] = NULL;
    }
    mgr->size = size;
    return mgr;
}

void free_fd_ctx_mgr(fd_ctx_mgr_t *mgr)
{
    for (int i = 0; i <= mgr->size; i++) {
        fd_ctx_t *ctx = mgr->fds[i];
        if (ctx) {
            decr_ref_fd_ctx(ctx);
        }
    }
    free(mgr);
}

static __thread fd_ctx_mgr_t * g_fd_ctx_mgr = NULL;

static inline
void expand(fd_ctx_mgr_t *mgr, int need_size)
{
    int size = mgr->size;
    while (size <= need_size) {
        size +=10000;
    }
    mgr->fds = (fd_ctx_t **)realloc(mgr->fds, size+1);
    for (int i = mgr->size+1; i < size; i++) {
        mgr->fds[i] = NULL;
    }
}

DEF_TLS_GET(g_fd_ctx_mgr, create_fd_ctx_mgr(10000), free_fd_ctx_mgr)


fd_ctx_t *get_fd_ctx(int fd, int type)
{
    if (fd <0) return NULL;

    fd_ctx_mgr_t *mgr = tls_get(g_fd_ctx_mgr);

    if (fd >  mgr->size) {
        assert("!too many fd");
        exit(1);
    }
    fd_ctx_t *ctx =   mgr->fds[fd];
    if (NULL == ctx) {
        return NULL;
    }

    if (type == 0) return ctx;

    if (ctx->type == type) {
        return ctx;
    }
    return NULL;
}

fd_ctx_t *alloc_fd_ctx(int fd, int type)
{
    if (fd <0) {
        assert(!"fd <0");
        exit(1);
        return NULL;
    }

    fd_ctx_mgr_t *mgr = tls_get(g_fd_ctx_mgr);

    if (fd >= mgr->size ) {
        expand(mgr, fd);
    }

    fd_ctx_t * d = mgr->fds[fd];
    if (NULL == d)
    {
        HOOK_SYS_FUNC(fcntl);
        d = ( fd_ctx_t *) malloc(sizeof(fd_ctx_t));
        d->type = type;
        d->fd = fd;
        d->use_cnt = 1;
        INIT_LIST_HEAD(&d->poll_wait_queue);
        d->rcv_timeout = 1000;
        d->snd_timeout = 1000;
        int flags = 0;
        flags = _(fcntl)(fd, F_GETFL, 0);
        d->user_flag = flags;
        d->wait_events = 0;

        // 设置 none block, 方便hook 系统调用
        // user_flag 只保存用户设置的标志。
        _(fcntl)(fd, F_SETFL, flags | O_NONBLOCK);
        mgr->fds[fd] = d;
    }
    return mgr->fds[fd];
}

int free_fd_ctx(int fd)
{
    if (fd <0) return -1;

    fd_ctx_mgr_t *mgr = tls_get(g_fd_ctx_mgr);

    if (fd > mgr->size ) {
        return -2;
    }

    fd_ctx_t * d = mgr->fds[fd];

    if (NULL == d) return -2;

    mgr->fds[fd] = NULL;

    decr_ref_fd_ctx(d);
    return 0;
}


void incr_ref_fd_ctx(fd_ctx_t *obj)
{
    ++obj->use_cnt;
}

void decr_ref_fd_ctx(fd_ctx_t *obj)
{
    --obj->use_cnt;
    if (obj->use_cnt <= 0) {
        free(obj);
    }
}

}
