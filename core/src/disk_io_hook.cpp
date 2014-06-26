/*
 * =====================================================================================
 *
 *       Filename:  disk_io_hook.cpp
 *
 *    Description:  
 *
 *        Version:  1.0
 *        Created:  2014年06月26日 22时38分35秒
 *       Revision:  none
 *       Compiler:  gcc
 *
 *         Author:  piboyeliu
 *   Organization:  
 *
 * =====================================================================================
 */
#include <stdlib.h>
#include <sys/types.h>
#include <unistd.h>
#include <sys/eventfd.h>
#include <stdio.h>      /*  for perror() */
#include <unistd.h>     /*  for syscall() */
#include <sys/syscall.h>    /*  for __NR_* definitions */
#include <fcntl.h>      /*  O_RDWR */
#include <string.h>     /*  memset() */
#include <inttypes.h>   /*  uint64_t */
#include <libaio.h>
#include <sys/ioctl.h>
#include <dlfcn.h>
#include <stdarg.h>
#include "coroutine.h"
#include "coroutine_impl.h"
#include "fd_ctx.h"
#include "network_hook.h"
#include <errno.h>
#include "tls.h"
#include "hook_helper.h"


using namespace conet;

HOOK_DECLARE(
    int, fcntl, (int fd, int cmd, ...)
);

HOOK_SYS_FUNC_DEF(int, open, (const char *pathname, int flags, ...))
{
	HOOK_SYS_FUNC( open );
    va_list vl;
    va_start(vl, flags);
    int mode = va_arg(vl, int);
    va_end(vl);
	if( !conet::is_enable_sys_hook() )
	{
		return _(open)(pathname, flags, mode);
	}

    int fd = _(open)(pathname, flags, mode);
	if( fd < 0 )
	{
		return fd;
	}
    fd_ctx_t *lp = alloc_fd_ctx(fd, 2);
    lp->user_flag = flags;
	fcntl( fd, F_SETFL, _(fcntl)(fd, F_GETFL,0 ) );
    return fd;
}

HOOK_SYS_FUNC_DEF(int, create, (const char *pathname, mode_t mode))
{
	HOOK_SYS_FUNC( create );
	if( !conet::is_enable_sys_hook() )
	{
		return _(create)(pathname, mode);
	}
    int fd = _(create)(pathname, mode);
	if( fd < 0 )
	{
		return fd;
	}
    fd_ctx_t *lp = alloc_fd_ctx(fd, 2);
    lp->user_flag = _(fcntl)(fd, F_GETFL,0 );
	fcntl( fd, F_SETFL, lp->user_flag);
    return fd;
}

HOOK_SYS_FUNC_DEF(int, openat, (int dirfd, const char *pathname, int flags, ...))
{
	HOOK_SYS_FUNC( openat );
    va_list vl;
    va_start(vl, flags);
    int mode = va_arg(vl, int);
    va_end(vl);
	if( !conet::is_enable_sys_hook() )
	{
		return _(openat)(dirfd, pathname, flags, mode);
	}
    int fd = _(openat)(dirfd, pathname, flags, mode);
	if( fd < 0 )
	{
		return fd;
	}
    fd_ctx_t *lp = alloc_fd_ctx(fd, 2);
    lp->user_flag = flags;
	fcntl( fd, F_SETFL, _(fcntl)(fd, F_GETFL,0 ) );
    return fd;
}

namespace 
{
struct disk_io_ctx_t
{
    io_context_t ctx;
    int num;
    int eventfd;
    conet::coroutine_t *proc_co;
};

struct conet_io_cb_t
{
    conet::coroutine_t *co;
    struct iocb *cb;
    struct io_event *event;
};

}

static __thread disk_io_ctx_t * g_disk_io_ctx = NULL;

static int proc_disk_event(void *arg)
{
    conet::enable_sys_hook(); 
    disk_io_ctx_t *ctx = (disk_io_ctx_t *)(arg);
    timespec ts;

    ts.tv_sec = 0;
    ts.tv_nsec = 0;
    struct io_event  event[64];
    do
    {
        int ready = 0;
        int n = 0;
        n = read(ctx->eventfd, &ready, 8);
        while ((n == 8) && ready > 0) {
            int events = io_getevents(ctx->ctx, 1, 64, event, &ts);
            if (events > 0) {
                ready -= events;
                for (int i = 0; i < events; i++) {
                    conet_io_cb_t * cb=  (conet_io_cb_t *)(event[i].data);
                    cb->event = event+i;
                    conet::resume(cb->co);
                }
                continue;
            }
            break;
        }
    } while(1);
    return 0;
}


static 
void fini_disk_io_ctx(void *arg)
{
   disk_io_ctx_t *ctx = (disk_io_ctx_t *)(arg);
   io_queue_release(ctx->ctx);
   delete ctx;
}

static
inline
disk_io_ctx_t * get_disk_ctx()
{
    if (NULL == g_disk_io_ctx) {
        g_disk_io_ctx = new disk_io_ctx_t();
        memset(g_disk_io_ctx, 0, sizeof(*g_disk_io_ctx));

        int ret=0;
        g_disk_io_ctx->ctx = 0;
        g_disk_io_ctx->num = 128;

        int evfd = 0; 
        evfd = eventfd(0, EFD_NONBLOCK);
        if (evfd <0) return NULL;
        g_disk_io_ctx->eventfd = evfd; 

        fd_ctx_t *ev_ctx = alloc_fd_ctx(evfd);
        ev_ctx->user_flag &= ~O_NONBLOCK;

        //ret = io_setup(g_disk_io_ctx->num, &g_disk_io_ctx->ctx); 
        ret = io_queue_init(g_disk_io_ctx->num, &g_disk_io_ctx->ctx); 
        if (ret) {
            close(evfd);
            delete g_disk_io_ctx;
            g_disk_io_ctx = NULL;
            conet::disable_sys_hook();
            exit(-1);
        }
        tls_onexit_add(g_disk_io_ctx, fini_disk_io_ctx);
    }
    return g_disk_io_ctx; 
}

static 
inline
void start_disk_task()
{
   disk_io_ctx_t *ctx = get_disk_ctx(); 
   if (ctx && (NULL == ctx->proc_co)) {
        ctx->proc_co=conet::alloc_coroutine(proc_disk_event, ctx);
        conet::resume(g_disk_io_ctx->proc_co);
   }
}

HOOK_DECLARE(
    ssize_t, read, ( int fd, void *buf, size_t nbyte )
);

HOOK_DECLARE(
    ssize_t, write, ( int fd, const void *buf, size_t nbyte )
);

namespace conet 
{
ssize_t disk_read(int fd, void *buf, size_t nbyte)
{
    fd_ctx_t *ctx = get_fd_ctx(fd,2);  
    if (NULL==ctx) {
        return _(read)(fd, buf, nbyte);
    }
    int ret = 0;

    off64_t off = 0;
    ret = lseek64(fd,  0, SEEK_CUR);
    if (ret) {
        return _(read)(fd, buf, nbyte);
    }


    struct disk_io_ctx_t * disk_ctx =   get_disk_ctx();
    struct iocb cb;
    memset(&cb, 0, sizeof(cb));

    conet_io_cb_t my_cb;
    my_cb.cb = &cb;
    io_prep_pread(&cb, fd, buf, nbyte, off);
    io_set_eventfd(&cb, disk_ctx->eventfd); 
    io_set_callback(&cb, (io_callback_t)(&my_cb)); 

    struct iocb *cbs[1]; 
    cbs[0] = &cb;
    ret = io_submit(disk_ctx->ctx, 1, cbs);
    if (ret != 1) {
        return  -1;
    }

    my_cb.co  = conet::current_coroutine();

    start_disk_task();
    conet::yield();

    unsigned long res = 0;
    unsigned long res2 = 0;
    res = my_cb.event->res;
    res2 = my_cb.event->res2;

    if (res2 != 0) {
        return -1; 
    }
    if (res != cb.u.c.nbytes) {
        return res;
    }

    return res;
}


ssize_t disk_write(int fd, const void *buf, size_t nbyte)
{
    fd_ctx_t *ctx = get_fd_ctx(fd,2);  
    if (NULL==ctx) {
        return _(write)(fd, buf, nbyte);
    }
    int ret = 0;

    off64_t off = 0;
    if (!(ctx->user_flag & O_APPEND)) {
        ret = lseek64(fd,  0, SEEK_CUR);
        if (ret) {
            return _(write)(fd, buf, nbyte);
        }
    }

    

    struct disk_io_ctx_t * disk_ctx =   get_disk_ctx();
    struct iocb cb;
    conet_io_cb_t my_cb;
    my_cb.cb = &cb;
    io_prep_pwrite(&cb, fd, (void *)buf, nbyte, off);
    io_set_eventfd(&cb, disk_ctx->eventfd); 
    io_set_callback(&cb, (io_callback_t)(&my_cb)); 

    struct iocb *cbs[1]; 
    cbs[0] = &cb;
    ret = io_submit(disk_ctx->ctx, 1, cbs);
    if (ret != 1) {
        return  -1;
    }

    my_cb.co  = conet::current_coroutine();
    start_disk_task();
    conet::yield();

    unsigned long res = 0;
    unsigned long res2 = 0;
    res = my_cb.event->res;
    res2 = my_cb.event->res2;

    if (res2 != 0) {
        return -1; 
    }
    if (res != cb.u.c.nbytes) {
        return res;
    }

    return res;
}

}

