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
#include <sys/stat.h>
#include <errno.h>
#include <linux/stat.h>

#include <dlfcn.h>
#include <stdarg.h>
#include "coroutine.h"
#include "coroutine_impl.h"
#include "fd_ctx.h"
#include "network_hook.h"
#include "base/incl/tls.h"
#include "hook_helper.h"


using namespace conet;

HOOK_DECLARE(
    int, fcntl, (int fd, int cmd, ...)
);

//HOOK_DECLARE(int ,fprintf,( FILE * fp, const char *format,...));

HOOK_SYS_FUNC_DEF(int, open, (const char *pathname, int flags, ...))
{
	HOOK_SYS_FUNC( open );
    int fd = 0;
    if (flags & O_CREAT)
    {
        va_list vl;
        va_start(vl, flags);
        mode_t mode = (mode_t) va_arg(vl, int);
        va_end(vl);
		fd = _(open)(pathname, flags, mode);
    } else {
        fd = _(open)(pathname, flags, DEFFILEMODE);
    }
	if( !conet::is_enable_sys_hook() )
	{
        return fd;
	}

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
    int fd = _(create)(pathname, mode);
	if( !conet::is_enable_sys_hook() || (fd <0))
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
    int fd = 0;
    if (flags & O_CREAT)
    {
        va_list vl;
        va_start(vl, flags);
        mode_t mode = (mode_t) va_arg(vl, int);
        va_end(vl);
		fd = _(openat)(dirfd, pathname, flags, mode);
    } else {
        fd = _(openat)(dirfd, pathname, flags, DEFFILEMODE);
    }
	if( !conet::is_enable_sys_hook() || (fd <0))
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
        uint64_t ready = 0;
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
    } while(1); // need dead loop
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
        conet::resume(ctx->proc_co);
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
    off = lseek64(fd,  0, SEEK_CUR);

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
    lseek64(fd, res, SEEK_CUR);
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
        off = lseek64(fd,  0, SEEK_CUR);
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
    lseek64(fd, res, SEEK_CUR);
    if (res != cb.u.c.nbytes) {
        return res;
    }
    return res;
}

}

HOOK_SYS_FUNC_DEF(
ssize_t ,pread,(int fd, void *buf, size_t nbyte, off_t off))
{
    HOOK_SYS_FUNC(pread);
    fd_ctx_t *ctx = get_fd_ctx(fd,2);  
    if (!conet::is_enable_sys_hook() || (NULL==ctx)) {
        return _(pread)(fd, buf, nbyte, off);
    }
    int ret = 0;


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

HOOK_DECLARE(
ssize_t , writev,(int fd, const struct iovec *iov, int iovcnt)
);
HOOK_DECLARE(
ssize_t , readv,(int fd, const struct iovec *iov, int iovcnt)
);
namespace conet
{

ssize_t disk_readv(int fd, const struct iovec *iov, int iovcnt)
{
    int ret = 0;
    off64_t off = 0;
    off = lseek64(fd,  0, SEEK_CUR);

    struct disk_io_ctx_t * disk_ctx =   get_disk_ctx();
    struct iocb cb;
    conet_io_cb_t my_cb;
    my_cb.cb = &cb;
    io_prep_preadv(&cb, fd, (struct iovec *)iov, iovcnt, off);
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

    lseek64(fd, res, SEEK_CUR);

    if (res != cb.u.c.nbytes) {
        return res;
    }

    return res;
}

ssize_t disk_writev(fd_ctx_t * ctx, int fd, const struct iovec *iov, int iovcnt)
{
    int ret = 0;

    off64_t off = 0;
    if (!(ctx->user_flag & O_APPEND)) {
        off = lseek64(fd,  0, SEEK_CUR);
    }

    struct disk_io_ctx_t * disk_ctx =   get_disk_ctx();
    struct iocb cb;
    conet_io_cb_t my_cb;
    my_cb.cb = &cb;
    io_prep_pwritev(&cb, fd, (struct iovec *)iov, iovcnt, off);
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

    lseek64(fd, res, SEEK_CUR);

    if (res != cb.u.c.nbytes) {
        return res;
    }

    return res;
}
}

HOOK_SYS_FUNC_DEF(
ssize_t , writev,(int fd, const struct iovec *iov, int iovcnt)
)
{
    HOOK_SYS_FUNC(writev);
    if (!conet::is_enable_sys_hook()) {
        return _(writev)(fd, iov, iovcnt);
    }
    fd_ctx_t *ctx = get_fd_ctx(fd,0);  
    if (!ctx || (O_NONBLOCK & ctx->user_flag )) 
    {
        return _(writev)(fd, iov, iovcnt);
    }
    if (ctx->type == 2) {
        //disk
        return disk_writev(ctx, fd, iov, iovcnt);
    }
    ssize_t ret = 0;
    ret = _(writev)(fd, iov, iovcnt);
    if (ret >=0) {
        return ret;
    }

    if (errno != EAGAIN) return ret;

    int timeout = ctx->snd_timeout;
    struct pollfd pf = {
        fd : fd,
        events:( POLLOUT | POLLERR | POLLHUP )
    };
    ret = poll( &pf,1,timeout );
    if (ret <= 0) {
        return -1;
    }
    ret = _(writev)(fd, iov, iovcnt);
    return ret;
}

HOOK_SYS_FUNC_DEF(
ssize_t , readv,(int fd, const struct iovec *iov, int iovcnt)
)
{
    HOOK_SYS_FUNC(readv);
    if (!conet::is_enable_sys_hook()) 
    {
        return _(readv)(fd, iov, iovcnt);
    }
    fd_ctx_t *ctx = get_fd_ctx(fd, 0);  
    if (!ctx || (O_NONBLOCK & ctx->user_flag )) 
    {
        return _(readv)(fd, iov, iovcnt);
    }
    if (ctx->type == 2) {
        //disk
        return disk_readv(fd, iov, iovcnt);
    }
    int timeout = ctx->rcv_timeout;

    struct pollfd pf = {
        fd: fd,
        events: POLLIN | POLLERR | POLLHUP
    };

    int ret = 0;
    ret = poll( &pf, 1, timeout );
    if (ret <= 0) {
        return -1;
    }

    ret = _(readv)( fd, iov, iovcnt);
    return ret;
}

HOOK_SYS_FUNC_DEF(
ssize_t , preadv,(int fd, const struct iovec *iov, int iovcnt, off_t off))
{
    HOOK_SYS_FUNC(preadv);
    fd_ctx_t *ctx = get_fd_ctx(fd,2);  
    if (!conet::is_enable_sys_hook() || (NULL==ctx)) {
        return _(preadv)(fd, iov, iovcnt, off);
    }
    int ret = 0;

    struct disk_io_ctx_t * disk_ctx =   get_disk_ctx();
    struct iocb cb;
    conet_io_cb_t my_cb;
    my_cb.cb = &cb;
    io_prep_preadv(&cb, fd, (struct iovec *)iov, iovcnt, off);
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


HOOK_SYS_FUNC_DEF(
ssize_t ,pwrite,(int fd, const void *buf, size_t nbyte, off_t off))
{
    HOOK_SYS_FUNC(pwrite);
    fd_ctx_t *ctx = get_fd_ctx(fd,2);  
    if (!conet::is_enable_sys_hook() || (NULL==ctx)) {
        return _(pwrite)(fd, buf, nbyte, off);
    }
    int ret = 0;


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



HOOK_SYS_FUNC_DEF(
ssize_t , pwritev,(int fd, const struct iovec *iov, int iovcnt, off_t off))
{
    fd_ctx_t *ctx = get_fd_ctx(fd,2);  
    if (!conet::is_enable_sys_hook() || (NULL==ctx)) {
        return _(pwritev)(fd, iov, iovcnt, off);
    }
    int ret = 0;


    struct disk_io_ctx_t * disk_ctx =   get_disk_ctx();
    struct iocb cb;
    conet_io_cb_t my_cb;
    my_cb.cb = &cb;
    io_prep_pwritev(&cb, fd, (struct iovec *)iov, iovcnt, off);
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
HOOK_SYS_FUNC_DEF(
int ,fsync,(int fd)
)
{
    HOOK_SYS_FUNC(fsync);
    if (!conet::is_enable_sys_hook()) {
        return _(fsync)(fd);
    }
    fd_ctx_t *ctx = get_fd_ctx(fd,2);  
    if (!ctx || (O_NONBLOCK & ctx->user_flag )) 
    {
        return _(fsync)(fd);
    }

    int ret =0;

    struct disk_io_ctx_t * disk_ctx =   get_disk_ctx();
    struct iocb cb;
    conet_io_cb_t my_cb;
    my_cb.cb = &cb;
    io_prep_fsync(&cb, fd);
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
    return 0;
}

HOOK_SYS_FUNC_DEF(
int ,fdatasync, (int fd) 
)
{
    HOOK_SYS_FUNC(fdatasync);
    if (!conet::is_enable_sys_hook()) {
        return _(fdatasync)(fd);
    }
    fd_ctx_t *ctx = get_fd_ctx(fd,2);  
    if (!ctx || (O_NONBLOCK & ctx->user_flag )) 
    {
        return _(fdatasync)(fd);
    }

    int ret =0;

    struct disk_io_ctx_t * disk_ctx =   get_disk_ctx();
    struct iocb cb;
    conet_io_cb_t my_cb;
    my_cb.cb = &cb;
    io_prep_fdsync(&cb, fd);
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
    return 0;
}


// FILE STREAM HOOK
//
#define CALC_FILE_POS(fp, fd) \
 do { \
     off64_t off = lseek64(fd, 0, SEEK_CUR); \
     fseek(fp, off, SEEK_SET); \
 }while(0)

#define __SRD 1
#define __SWR 2 
#define __SRW 3 
static
int __sflags(const char *mode, int *optr)
{
    int ret, m, o;

    switch (*mode++) {

    case 'r':   /* open for reading */
        ret = __SRD;
        m = O_RDONLY;
        o = 0;
        break;

    case 'w':   /* open for writing */
        ret = __SWR;
        m = O_WRONLY;
        o = O_CREAT | O_TRUNC;
        break;

    case 'a':   /* open for appending */
        ret = __SWR;
        m = O_WRONLY;
        o = O_CREAT | O_APPEND;
        break;

    default:    /* illegal mode */
        errno = EINVAL;
        return (0);
    }

    /* [rwa]\+ or [rwa]b\+ means read and write */
    if (*mode == '+' || (*mode == 'b' && mode[1] == '+')) {
        ret = __SRW;
        m = O_RDWR;
    }
    *optr = m | o;
    return (ret);
}


HOOK_SYS_FUNC_DEF(
FILE * ,fopen,(const char *file, const char *mode)
)
{

    HOOK_SYS_FUNC(fopen);
    if (!conet::is_enable_sys_hook()) 
    {
        return _(fopen)(file, mode);
    }

	int fd;
	int flags, oflags;
	if ((flags = __sflags(mode, &oflags)) == 0)
		return (NULL);
    if ((oflags & O_RDONLY) || (oflags & O_RDWR))
    {   // read file, need buff, not hook
        return _(fopen)(file, mode);
    }

    if (!(oflags & O_APPEND)) {
        return _(fopen)(file, mode);
    }

	if ((fd = open(file, oflags, DEFFILEMODE)) < 0) {
		return (NULL);
	}

    FILE *fp =  fdopen(fd, mode);
    if (oflags & O_APPEND)
            fseek(fp, 0, SEEK_END);
    return fp;
}


// TODO: need to hook freopen
//

HOOK_SYS_FUNC_DEF(
int  ,fclose,(FILE *fp)
)
{

    HOOK_SYS_FUNC(fclose);
    if (!conet::is_enable_sys_hook()) 
    {
        return _(fclose)(fp);
    }
    int fd = fileno(fp);
    conet::free_fd_ctx(fd);
    return _(fclose)(fp);
}

HOOK_SYS_FUNC_DEF(
int ,vfprintf ,( FILE * fp, const char * format, va_list ap )
)
{
    HOOK_SYS_FUNC(vfprintf);

    int ret = 0;

    if (!conet::is_enable_sys_hook()) {
        ret = _(vfprintf)(fp, format, ap);
        return ret;
    }
    int fd = fileno(fp);
    fd_ctx_t *ctx = get_fd_ctx(fd,2);  
    if (!ctx || (O_NONBLOCK & ctx->user_flag )) 
    {
        ret =  _(vfprintf)(fp, format, ap);
        return ret;
    }
    char buf[100];
    char *p = buf;
    int len = 99;
    int nlen = 0;
    va_list bak_arg;
    va_copy(bak_arg, ap);
    nlen = vsnprintf( p, len,  format, ap);
    va_end(ap);
    if (nlen > len) {
        p = (char *)malloc(nlen+1);
        len = nlen;
        nlen = vsnprintf(p, len, format, bak_arg);
        va_end(bak_arg);
    }
    ret = conet::disk_write(fd, p, nlen);
    if (p != buf) {
        free(p);
    }
    CALC_FILE_POS(fp, fd);
    return ret;
}

// fprintf(fp, "abc") would replace to fwrite;
HOOK_SYS_FUNC_DEF(
int ,fprintf,(FILE * fp, const char *format, ...)
)
{
    HOOK_SYS_FUNC(fprintf);

    int ret = 0;
    va_list ap;
    va_start(ap, format);

    if (!conet::is_enable_sys_hook()) {
        ret = _(vfprintf)(fp, format, ap);
        va_end(ap);
        return ret;
    }
    int fd = fileno(fp);
    fd_ctx_t *ctx = get_fd_ctx(fd,2);  
    if (!ctx || (O_NONBLOCK & ctx->user_flag )) 
    {
        ret =  _(vfprintf)(fp, format, ap);
        va_end(ap);
        return ret;
    }
    char buf[100];
    char *p = buf;
    int len = 99;
    int nlen = 0;
    nlen = vsnprintf( p, len,  format, ap);
    if (nlen > len) {
        va_end(ap);
        va_start(ap, format);
        p = (char *)malloc(nlen+1);
        len = nlen;
        nlen = vsnprintf(p, len, format, ap);
    }
    va_end(ap);
    ret = 0;
    ret = conet::disk_write(fd, p, nlen);
    if (p != buf) {
        free(p);
    }
    CALC_FILE_POS(fp, fd);
    return ret;
} 


//fputs("abc", fp) would replace to fwrite;

HOOK_SYS_FUNC_DEF(
int , fputs,(const char *str, FILE *fp)
)
{
    HOOK_SYS_FUNC(fputs);
    if (!conet::is_enable_sys_hook()) {
        return _(fputs)(str, fp);
    }
    int fd = fileno(fp);
    fd_ctx_t *ctx = get_fd_ctx(fd,2);  
    if (!ctx || (O_NONBLOCK & ctx->user_flag )) 
    {
        return _(fputs)(str, fp);
    }
    size_t len = strlen(str);
    int ret= 0;
    ret = conet::disk_write(fd, str, len);
    CALC_FILE_POS(fp, fd);
    return ret;
}

HOOK_SYS_FUNC_DEF(
int , fputc,(int c, FILE *fp)
)
{
    HOOK_SYS_FUNC(fputc);
    if (!conet::is_enable_sys_hook()) {
        return _(fputc)(c, fp);
    }
    int fd = fileno(fp);
    fd_ctx_t *ctx = get_fd_ctx(fd,2);  
    if (!ctx || (O_NONBLOCK & ctx->user_flag )) 
    {
        return _(fputc)(c, fp);
    }
    char ch = char(c);
    int ret =  conet::disk_write(fd, &ch, 1);
    CALC_FILE_POS(fp, fd);
    return ret;
}


HOOK_SYS_FUNC_DEF(
 size_t ,fwrite,(const void *ptr, size_t size, size_t nmemb,
                          FILE *fp)
)
{
    HOOK_SYS_FUNC(fwrite);
    if (!conet::is_enable_sys_hook()) {
        return _(fwrite)(ptr, size, nmemb, fp);
    }
    int fd = fileno(fp);
    fd_ctx_t *ctx = get_fd_ctx(fd,2);  
    if (!ctx || (O_NONBLOCK & ctx->user_flag )) 
    {
        return _(fwrite)(ptr, size, nmemb, fp);
    }
    int ret = conet::disk_write(fd, ptr, size*nmemb);
    if (ret<0) {
        return ret;
    }
    int failed_bytes =  ret % size;
    off64_t off = lseek64(fd, 0, SEEK_CUR); 
    off = off - failed_bytes;
    fseek(fp, off, SEEK_SET); 
    return ret/size;
}

/*
// read operator, carefull, because has other read function no hook, buff would error
HOOK_SYS_FUNC_DEF(
 size_t ,fread,(void *ptr, size_t size, size_t nmemb,
                          FILE *fp)
)
{
    HOOK_SYS_FUNC(fread);
    if (!conet::is_enable_sys_hook()) {
        return _(fread)(ptr, size, nmemb, fp);
    }
    int fd = fileno(fp);
    fd_ctx_t *ctx = get_fd_ctx(fd,2);  
    if (!ctx || (O_NONBLOCK & ctx->user_flag )) 
    {
        return _(fread)(ptr, size, nmemb, fp);
    }
    int ret = conet::disk_read(fd, ptr, size*nmemb);
    if (ret <0) return ret;
    int failed_bytes =  ret % size;
    off64_t off = lseek64(fd, 0, SEEK_CUR); 
    off = off - failed_bytes;
    fseek(fp, off, SEEK_SET); 
    return ret/size;
}  
*/
