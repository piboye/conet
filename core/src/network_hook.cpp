#include <sys/socket.h>
#include <sys/time.h>
#include <sys/syscall.h>
#include <sys/un.h>

#include <poll.h>
#include <unistd.h>
#include <fcntl.h>

#include <netinet/in.h>
#include <errno.h>
#include <time.h>

#include <stdio.h>
#include <string.h>
#include <stdlib.h>
#include <stdint.h>
#include <stdarg.h>
#include <pthread.h>
#include <vector>
#include <time.h>
#include "ares_wrap.h"

#include "network_hook.h"
#include "dispatch.h"
#include "hook_helper.h"


#include "coroutine.h"
#include "coroutine_env.h"
#include "fd_ctx.h"
#include "log.h"

using conet::alloc_fd_ctx;
using conet::get_fd_ctx;
using conet::free_fd_ctx;
using conet::fd_ctx_t;

namespace conet
{

__attribute__ ((weak)) ssize_t disk_readv(int fd, const struct iovec *iov, int iovcnt);

__attribute__((weak))
ssize_t disk_writev(fd_ctx_t * ctx, int fd, const struct iovec *iov, int iovcnt);
}
namespace conet
{
void close_fd_notify_poll(int fd);
}

namespace conet 
{
__attribute__ ((weak))
ssize_t disk_read(int fd, void *buf, size_t nbyte);

__attribute__((weak))
ssize_t disk_write(int fd, const void *buf, size_t nbyte);

int  my_accept4( int fd, struct sockaddr *addr, socklen_t *len, int flags);

}

extern "C" 
{


HOOK_SYS_FUNC_DEF(
    int, socket, (int domain, int type, int protocol)
)
{
    HOOK_SYS_FUNC(socket);
    if( !conet::is_enable_sys_hook() )
    {
        return _(socket)(domain, type, protocol);
    }

    int fd = _(socket)(domain, type, protocol);
    if( fd < 0 )
    {
        return fd;
    }

    fd_ctx_t *lp = alloc_fd_ctx( fd );
    lp->domain = domain;

    return fd;
}


HOOK_SYS_FUNC_DEF(
    int , accept, ( int fd, struct sockaddr *addr, socklen_t *len)
)
{
    HOOK_SYS_FUNC(accept);
    if( !conet::is_enable_sys_hook() )
    {
        return _(accept)(fd, addr, len);
    }


    int client_fd = -1;

    fd_ctx_t *lp = get_fd_ctx( fd );

    if( !lp || ( O_NONBLOCK & lp->user_flag ) )
    {
        client_fd =   _(accept)(fd, addr, len);
    } else {
        //block call

        struct pollfd pf = { fd: fd, events: POLLIN|POLLERR|POLLHUP};
        int ret = conet::co_poll( &pf,1, -1);
        if (ret == 0) {
            errno = ETIMEDOUT;
            return -1;
        }
        if (ret <0) {
            return -1;
        }

        if (pf.revents & POLLERR) {
            return -1;
        }
        if (pf.revents & POLLHUP) {
            return -1;
        }

        client_fd =  _(accept)(fd, addr, len);
    }
    if (client_fd >=0) {
        fd_ctx_t *ctx = alloc_fd_ctx(client_fd);
        if (lp) {
            ctx->domain = lp->domain;
        }
    }
    return client_fd;
}

HOOK_SYS_FUNC_DEF(
    int , accept4, ( int fd, struct sockaddr *addr, socklen_t *len, int flags)
)
{
    HOOK_SYS_FUNC(accept4);
    if( !conet::is_enable_sys_hook() )
    {
        return _(accept4)(fd, addr, len, flags);
    }

    int client_fd = -1;
    fd_ctx_t *lp = get_fd_ctx( fd );

    if( !lp || ( O_NONBLOCK & lp->user_flag ) )
    {
        client_fd =   _(accept4)(fd, addr, len, flags);
    } 
    else 
    {

        //block call
        struct pollfd pf = { fd: fd, events: POLLIN|POLLERR|POLLHUP };
        int ret = conet::co_poll( &pf,1, -1);
        if (ret == 0) {
            errno = ETIMEDOUT;
            return -1;
        }
        if (ret <0) {
            return -1;
        }
        if (pf.revents & POLLERR) {
            return -1;
        }
        if (pf.revents & POLLHUP) {
            return -1;
        }
        client_fd =  _(accept4)(fd, addr, len, flags);
    }
    if (client_fd >=0) {
        fd_ctx_t *ctx = NULL;
        if (flags & O_NONBLOCK) {
            ctx = conet::alloc_fd_ctx2(client_fd, 1, true);
            ctx->user_flag = O_NONBLOCK;
        } else {
            ctx = conet::alloc_fd_ctx(client_fd, 1);
        }
        if (lp) {
            ctx->domain = lp->domain;
        }
        //ctx->user_flag = flags;
    }
    return client_fd;

}

HOOK_SYS_FUNC_DEF(
    int ,connect, (int fd, const struct sockaddr *address, socklen_t address_len)
)
{
    HOOK_SYS_FUNC(connect);
    if ( !conet::is_enable_sys_hook() )
    {
        return _(connect)(fd,address,address_len);
    }

    int ret = 0;
    fd_ctx_t *lp = get_fd_ctx( fd );

    ret =  _(connect)(fd,address,address_len);
    if ( lp )
    {
        if(sizeof(lp->dest) >= address_len )
        {
            memcpy( &(lp->dest),address,(int)address_len );
        }
    }

    if (ret >=0 || O_NONBLOCK & lp->user_flag )  {
        return ret;
    }

    if (errno != EINPROGRESS)
    {
        return ret;
    }

    struct pollfd pf = { fd: fd, events: POLLIN|POLLOUT|POLLERR|POLLHUP};

    int poll_ret = poll ( &pf,1, lp->snd_timeout);
    if (0 == poll_ret) {
        // timeout
        errno = ETIMEDOUT;
        return -1;
    }
    if ( poll_ret <0) {
        return -1;
    }

    if (pf.revents & POLLERR) {
        return -1;
    }
    if (pf.revents & POLLHUP) {
        return -1;
    }

    ret = _(connect)(fd, address, address_len);
    if (ret == -1) {
        if (errno == EINPROGRESS) {
            errno = ETIMEDOUT;
        }
    }
    return ret;
}


HOOK_SYS_FUNC_DEF(
    int, close, (int fd)
)
{
    conet::close_fd_notify_poll(fd);
    HOOK_SYS_FUNC(close);
    if( !conet::is_enable_sys_hook() )
    {
        return syscall(SYS_close, fd );
    }

    if (fd < 0) return EBADF; 

    fd_ctx_t *lp = get_fd_ctx(fd);
    if (lp) 
    {
        free_fd_ctx( fd );
    }
    //close  必须在 free_fd_ctx 的前面
    int ret = syscall(SYS_close, fd);
    return ret;
}


HOOK_SYS_FUNC_DEF(
    ssize_t, read, ( int fd, void *buf, size_t nbyte )
)
{

    if( !conet::is_enable_sys_hook() ) {
        return syscall(SYS_read, fd, buf, nbyte ); // tcmalloc imcompatibility with dlsym, in global contructor;
    }

    fd_ctx_t *lp = get_fd_ctx( fd, 0);
    ssize_t ret = 0;

    if( !lp || ( O_NONBLOCK & lp->user_flag ) )
    {
        ret = _(read)(fd, buf, nbyte);
        return ret;
    }

    if (lp->type == 2)
    {
        if (conet::disk_read) {
            return conet::disk_read(fd, buf, nbyte);
        } else {
            return _(read)(fd, buf, nbyte);
        }
    }

    int timeout = lp->rcv_timeout;

    struct pollfd pf = {
        fd: fd,
        events: POLLIN | POLLERR | POLLHUP
    };

    // 预读取
    ret = syscall(SYS_read,  fd,(char*)buf , nbyte);
    //ret = _(read)(fd,(char*)buf , nbyte);
    if (ret >=0) {
        return ret;
    }
    if (errno != EAGAIN || errno != EINTR)
    {
        return ret;
    }

    ret = conet::co_poll( &pf, 1, timeout );
    if (ret == 0) {
        errno = ETIMEDOUT;
        return -1;
    }
    if (ret <0) {
        return -1;
    }

    if (pf.revents & POLLERR) {
        return -1;
    }

    if (pf.revents & POLLHUP) {
        return 0;
    }

    ret = syscall(SYS_read,  fd,(char*)buf , nbyte);
    //ret = _(read)(fd,(char*)buf , nbyte);
    return ret;
}

HOOK_SYS_FUNC_DEF(
    ssize_t, write, ( int fd, const void *buf, size_t nbyte )
)
{
    HOOK_SYS_FUNC(write);
    if( !conet::is_enable_sys_hook() )
    {
        return _(write)( fd,buf,nbyte );
    }

    ssize_t ret = 0;

    fd_ctx_t *lp = get_fd_ctx( fd, 0);

    if( !lp || ( O_NONBLOCK & lp->user_flag ) )
    {
        ret = _(write)( fd,buf,nbyte );
        return ret;
    }

    if (lp->type == 2)
    {
        if (conet::disk_write) {
            return conet::disk_write(fd, buf, nbyte);
        }else {
            return _(write)(fd, buf, nbyte);
        }
    }

    ret = _(write)(fd, (const char*) buf, nbyte);
    if (ret >=0) {
        return ret;
    }

    if (errno != EAGAIN) return ret;

    int timeout = lp->snd_timeout;
    struct pollfd pf = { fd : fd, events: ( POLLOUT | POLLERR | POLLHUP ) };
    ret = conet::co_poll( &pf,1,timeout );
    if (ret == 0) {
        errno = ETIMEDOUT;
        return -1;
    }
    if (ret <0) {
        return -1;
    }
    if (pf.revents & POLLERR) {
        return -1;
    }
    if (pf.revents & POLLHUP) {
        return 0;
    }
    ret = _(write)(fd, (const char*)buf, nbyte);
    return ret;
}


HOOK_SYS_FUNC_DEF(
ssize_t , writev,(int fd, const struct iovec *iov, int iovcnt)
)
{
    HOOK_SYS_FUNC(writev);
    if (!conet::is_enable_sys_hook()) {
        return _(writev)(fd, iov, iovcnt);
    }
    fd_ctx_t *ctx = get_fd_ctx(fd, 0);  
    if (!ctx || (O_NONBLOCK & ctx->user_flag )) 
    {
        return _(writev)(fd, iov, iovcnt);
    }
    if (ctx->type == 2) {
        //disk
        if (conet::disk_writev) {
            return disk_writev(ctx, fd, iov, iovcnt);
        }else {
            return _(writev)(fd, iov, iovcnt);
        }
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
    ret = conet::co_poll( &pf,1,timeout );
    if (ret <= 0) {
        return -1;
    }
    if (pf.revents & POLLERR) {
        return -1;
    }
    if (pf.revents & POLLHUP) {
        return 0;
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
        if (conet::disk_readv)
            return conet::disk_readv(fd, iov, iovcnt);
        else 
            return _(readv)(fd, iov, iovcnt);
    }
    int timeout = ctx->rcv_timeout;

    struct pollfd pf = {
        fd: fd,
        events: POLLIN | POLLERR | POLLHUP
    };

    int ret = 0;
    ret = conet::co_poll( &pf, 1, timeout );
    if (ret <= 0) {
        return -1;
    }

    if (pf.revents & POLLERR) {
        return -1;
    }

    if (pf.revents & POLLHUP) {
        return 0;
    }

    ret = _(readv)( fd, iov, iovcnt);
    return ret;
}


HOOK_SYS_FUNC_DEF(
    ssize_t, sendto, (int fd, const void *message, size_t length,
                      int flags, const struct sockaddr *dest_addr,
                      socklen_t dest_len)
)
{
    HOOK_SYS_FUNC(sendto);
    flags |= MSG_NOSIGNAL;
    if( !conet::is_enable_sys_hook() )
    {
        return _(sendto)(fd, message, length, flags, dest_addr, dest_len);
    }

    fd_ctx_t *lp = get_fd_ctx(fd);
    if( !lp || ( O_NONBLOCK & lp->user_flag ) || flags & MSG_DONTWAIT)
    {
        return _(sendto)(fd,message,length,flags,dest_addr,dest_len );
    }

    ssize_t ret = _(sendto)(fd, message, length, flags, dest_addr, dest_len);
    if( ret < 0 && EAGAIN == errno )
    {
        int timeout = lp->snd_timeout;

        struct pollfd pf = {fd: fd, events: ( POLLOUT | POLLERR | POLLHUP ) };
        ret = conet::co_poll(&pf, 1, timeout);
        if (ret == 0) {
            errno = ETIMEDOUT;
            return -1;
        }
        if (ret <0) {
            return -1;
        }

        if (pf.revents & POLLERR) {
            return -1;
        }
        if (pf.revents & POLLHUP) {
            return 0;
        }

        ret = _(sendto)(fd, message, length, flags, dest_addr, dest_len);
    }
    return ret;
}

HOOK_SYS_FUNC_DEF(
    ssize_t, recvfrom, (int fd, void *buffer, size_t length,
                        int flags, struct sockaddr *address,
                        socklen_t *address_len)
)
{
    HOOK_SYS_FUNC(recvfrom);
    if( !conet::is_enable_sys_hook() )
    {
        return _(recvfrom)(fd, buffer, length, flags, address, address_len);
    }

    fd_ctx_t *lp = get_fd_ctx(fd);
    if( !lp || ( O_NONBLOCK & lp->user_flag ) || flags & MSG_DONTWAIT)
    {
        return _(recvfrom)(fd, buffer, length, flags, address, address_len);
    }

    int timeout = lp->rcv_timeout;

    struct pollfd pf = { fd:fd, events:( POLLIN | POLLERR | POLLHUP ) };
    ssize_t ret = conet::co_poll( &pf,1,timeout );
    if (ret == 0) {
        errno = ETIMEDOUT;
        return -1;
    }
    if (ret < 0) {
        return -1;
    }


    if (pf.revents & POLLERR) {
        return -1;
    }
    if (pf.revents & POLLHUP) {
        return 0;
    }

    ret = _(recvfrom)(fd, buffer, length, flags, address, address_len);
    return ret;
}

HOOK_SYS_FUNC_DEF(
    ssize_t, send, (int fd, const void *buffer, size_t length, int flags)
)
{
    flags |= MSG_NOSIGNAL;
    HOOK_SYS_FUNC(send);
    if( !conet::is_enable_sys_hook() )
    {
        return _(send)(fd, buffer, length, flags);
    }
    fd_ctx_t *lp = get_fd_ctx(fd);

    if( !lp || ( O_NONBLOCK & lp->user_flag ) || flags & MSG_DONTWAIT)
    {
        return _(send)(fd, buffer, length, flags);
    }
    int timeout = lp->snd_timeout;

    ssize_t ret = _(send)(fd, buffer, length, flags);
    if (ret == -1 && errno == EAGAIN) {
        struct pollfd pf = { 0 };
        pf.fd = fd;
        pf.events = ( POLLOUT | POLLERR | POLLHUP );
        int ret = conet::co_poll( &pf,1,timeout );
        if (ret == 0) {
            errno = ETIMEDOUT;
            return -1;
        }
        if (ret < 0) {
            return -1;
        }


        if (pf.revents & POLLERR) {
            return -1;
        }
        if (pf.revents & POLLHUP) {
            return 0;
        }

        ret = _(send)(fd, (const char*)buffer, length, flags);
    }

    return ret;
}

HOOK_SYS_FUNC_DEF(
    ssize_t, recv, ( int fd, void *buffer, size_t length, int flags )
)
{

    HOOK_SYS_FUNC(recv);
    if( !conet::is_enable_sys_hook() )
    {
        return _(recv)(fd, buffer, length, flags);
    }
    fd_ctx_t *lp = get_fd_ctx(fd);

    if( !lp || ( O_NONBLOCK & lp->user_flag ) || flags & MSG_DONTWAIT)
    {
        return _(recv)(fd, buffer, length, flags);
    }
    int timeout = lp->rcv_timeout;

    ssize_t ret  = 0;
    ret = _(recv)(fd, buffer, length, flags);
    if (ret >=0) {
        return ret;
    }
    if (errno != EAGAIN) {
        return ret;
    }

    struct pollfd pf = { 0 };
    pf.fd = fd;
    pf.events = ( POLLIN | POLLERR | POLLHUP );
    ret = conet::co_poll( &pf,1, timeout );
    if (ret == 0) {
        errno = ETIMEDOUT;
        return -1;
    }

    if (ret < 0) {
        return -1;
    }

    if (pf.revents & POLLERR) {
        return -1;
    }

    if (pf.revents & POLLHUP) {
        return 0;
    }

    ret = _(recv)(fd, buffer, length, flags);
    return ret;
}



HOOK_SYS_FUNC_DEF(
    int, poll,(struct pollfd fds[], nfds_t nfds, int timeout)
)
{
    HOOK_SYS_FUNC(poll);
    if( !conet::is_enable_sys_hook() ) {
        return _(poll)( fds,nfds,timeout );
    }

    if(0 == timeout)
    {
        if (nfds <=0) {
            usleep(0);
            return 0;
        }
        return _(poll)(fds, nfds, 0);
    }

    return conet::co_poll(fds,nfds, timeout);
}

HOOK_SYS_FUNC_DEF(
    int, ppoll, (struct pollfd *fds, nfds_t nfds,
                 const struct timespec *timeout_ts, const sigset_t *sigmask)
)
{
    HOOK_SYS_FUNC(ppoll);
    if( !conet::is_enable_sys_hook() ) {
        return _(ppoll)(fds, nfds, timeout_ts, sigmask);
    }

    if(NULL == timeout_ts || (timeout_ts->tv_sec == 0 && timeout_ts->tv_nsec == 0))
    {
        if (nfds <=0) {
            usleep(0);
            return 0;
        }
        return _(ppoll)(fds, nfds, timeout_ts, sigmask);
    }

    int timeout = timeout_ts->tv_sec*1000 + timeout_ts->tv_nsec/1000000;
    return conet::co_poll(fds,nfds, timeout);
}

HOOK_SYS_FUNC_DEF(
    int, epoll_wait, (int epfd, struct epoll_event *events,
                      int maxevents, int timeout)
)
{
    HOOK_SYS_FUNC(epoll_wait);
    if( !conet::is_enable_sys_hook() ) {
        return _(epoll_wait)(epfd, events, maxevents, timeout);
    }

    if(timeout ==0)
    {
        if (maxevents <=0) {
            usleep(0);
            return 0;
        }
        return _(epoll_wait)(epfd, events, maxevents, timeout);
    }

    int ret = 0;
    int rest = timeout;
    while (1)
    {
        ret = _(epoll_wait)(epfd, events, maxevents, 0);
        if (ret != 0) {
            return ret;
        }
        if (timeout <0)
        {
            conet::co_poll(NULL, 0, 1);
            continue;
        }

        if (rest > 0)  {
            conet::co_poll(NULL, 0, 1);
        } else {
            return 0;
        }
        --rest;
    }
    return 0;
}

HOOK_SYS_FUNC_DEF(
    int, epoll_pwait, (int epfd, struct epoll_event *events,
                       int maxevents, int timeout, const sigset_t *sigmask)
)
{
    HOOK_SYS_FUNC(epoll_wait);
    if( !conet::is_enable_sys_hook() ) {
        return _(epoll_pwait)(epfd, events, maxevents, timeout, sigmask);
    }

    if(timeout ==0)
    {
        return _(epoll_pwait)(epfd, events, maxevents, timeout, sigmask);
    }

    int ret = 0;
    int rest = timeout;
    while (1)
    {
        ret = _(epoll_pwait)(epfd, events, maxevents, 0, sigmask);
        if (ret != 0) {
            return ret;
        }
        if (timeout <0)
        {
            conet::co_poll(NULL, 0, 1);
            continue;
        }

        if (rest > 0)  {
            conet::co_poll(NULL, 0, 1);
        } else {
            return 0;
        }
        --rest;
    }
    return 0;
}



HOOK_SYS_FUNC_DEF(
    int, setsockopt, (int fd, int level, int option_name,
                      const void *option_value, socklen_t option_len)
)
{

    HOOK_SYS_FUNC(setsockopt);
    if( !conet::is_enable_sys_hook() )
    {
        return _(setsockopt)(fd, level, option_name, option_value, option_len);
    }
    fd_ctx_t *lp = get_fd_ctx(fd);

    if( lp && SOL_SOCKET == level )
    {
        if( SO_RCVTIMEO == option_name  && option_len == sizeof(struct timeval))
        {
            struct timeval * t1 = (struct timeval *)(option_value);
            lp->rcv_timeout = t1->tv_sec*1000 + t1->tv_usec/1000;
        }
        else if( SO_SNDTIMEO == option_name && option_len == sizeof(struct timeval))
        {
            struct timeval * t1 = (struct timeval *)(option_value);
            lp->snd_timeout = t1->tv_sec*1000 + t1->tv_usec/1000;
        }
    }
    return _(setsockopt)(fd, level, option_name, option_value, option_len);
}

HOOK_SYS_FUNC_DEF(
    int, fcntl, (int fd, int cmd, ...)
)
{

    HOOK_SYS_FUNC(fcntl);
    if( fd < 0 ) {
        return -1;
    }

    va_list arg_list;
    va_start(arg_list, cmd);

    int ret = -1;
    fd_ctx_t *lp = get_fd_ctx( fd );
    switch( cmd )
    {
    case F_DUPFD:
    {
        int param = va_arg(arg_list,int);
        ret = (fcntl)( fd,cmd,param );
        if (ret >=0 && lp && conet::is_enable_sys_hook())
        {
            fd_ctx_t *lp0 = alloc_fd_ctx( ret );
            lp0->domain = lp->domain;
            lp0->rcv_timeout = lp->rcv_timeout;
            lp0->snd_timeout = lp->snd_timeout;
            lp0->user_flag = lp->user_flag;
        }
        break;
    }
    case F_GETFD:
    {
        ret = _(fcntl)( fd,cmd );
        break;
    }
    case F_SETFD:
    {
        int param = va_arg(arg_list,int);
        ret = _(fcntl)( fd,cmd,param );
        break;
    }
    case F_GETFL:
    {
        ret = _(fcntl)( fd,cmd );
        if (lp && (!(lp->user_flag & O_NONBLOCK ) && (ret & O_NONBLOCK)))
        {
            ret &= ~O_NONBLOCK;
        }
        break;
    }
    case F_SETFL:
    {
        int param = va_arg(arg_list,int);
        int flag = param;
        if( conet::is_enable_sys_hook() && lp )
        {
            flag |= O_NONBLOCK;
        }
        ret = _(fcntl)( fd,cmd,flag );
        if( 0 == ret && lp )
        {
            lp->user_flag = param;
        }
        break;
    }
    case F_GETOWN:
    {
        ret = _(fcntl)( fd, cmd);
        break;
    }
    case F_SETOWN:
    {
        int param = va_arg(arg_list,int);
        ret = _(fcntl)( fd,cmd,param );
        break;
    }
    case F_GETLK:
    {
        struct flock *param = va_arg(arg_list,struct flock *);
        ret = _(fcntl)( fd,cmd,param );
        break;
    }
    case F_SETLK:
    {
        struct flock *param = va_arg(arg_list,struct flock *);
        ret = _(fcntl)( fd,cmd,param );
        break;
    }
    case F_SETLKW:
    {
        struct flock *param = va_arg(arg_list,struct flock *);
        ret = _(fcntl)( fd,cmd,param );
        break;
    }
    }

    va_end( arg_list );

    return ret;
}

HOOK_SYS_FUNC_DEF(int , eventfd, (unsigned int initval, int flags))
{
	HOOK_SYS_FUNC( socket );

	if( !conet::is_enable_sys_hook() )
	{
		return _(eventfd)(initval, flags);
	}
	int fd = _(eventfd)(initval, flags);
	if( fd < 0 )
	{
		return fd;
	}

	fd_ctx_t *lp = alloc_fd_ctx( fd );
	lp->domain = 0;

    if (flags & O_NONBLOCK)
    {
        lp->user_flag |= O_NONBLOCK;
	    fcntl( fd, F_SETFL, _(fcntl)(fd, F_GETFL, 0));
    }
    else
    {
        fcntl( fd, F_SETFL, _(fcntl)(fd, F_GETFL, 0)& ~O_NONBLOCK);
    }

	return fd;
}

HOOK_SYS_FUNC_DEF(int ,nanosleep,(const struct timespec *req, struct timespec *rem))
{
    //HOOK_SYS_FUNC(nanosleep);
    if( !conet::is_enable_sys_hook() )
    {
        return syscall(SYS_nanosleep, req, rem);
    }

    if (NULL == req) { 
        errno = EINVAL;
        return -1;
    }

    int ms = req->tv_sec *1000+ (req->tv_nsec+999999)/1000000;

    if (ms == 0) {
        conet::delay_back();
        if (rem) {
            rem->tv_sec = 0;
            rem->tv_nsec = 0;
        }
        return 0;
    } 

    conet::co_poll(NULL, 0, ms);
    if (rem) {
        rem->tv_sec = 0;
        rem->tv_nsec = 0;
    }
    return 0;
}

HOOK_SYS_FUNC_DEF(unsigned int, sleep, (unsigned int s))
{
    HOOK_SYS_FUNC(sleep);
    if( !conet::is_enable_sys_hook() )
    {
        return _(sleep)(s);
    }

    if (s == 0) {
        conet::delay_back();
        return 0;
    } 

    return conet::co_poll(NULL, 0, s*1000);
}

HOOK_SYS_FUNC_DEF(int, usleep, (useconds_t us))
{

    HOOK_SYS_FUNC(usleep);
    if( !conet::is_enable_sys_hook() )
    {
        return _(usleep)(us);
    }

    if (us == 0) {
        conet::delay_back();
        return 0;
    } 

    int ms = (us+999)/1000;
    return conet::co_poll(NULL, 0, ms);
}

HOOK_SYS_FUNC_DEF(int, dup, (int old))
{

    HOOK_SYS_FUNC( dup );

    if( !conet::is_enable_sys_hook() )
    {   
        return _(dup)(old);
    }   
    int fd = _(dup)(old);
    if( fd < 0 ) 
    {   
        return fd; 
    }   
    fd_ctx_t *lp0 = get_fd_ctx(old);

    if (!lp0) return fd; 

    fd_ctx_t *lp = alloc_fd_ctx( fd );
    lp->domain = lp0->domain;
    lp->rcv_timeout = lp0->rcv_timeout;
    lp->snd_timeout = lp0->snd_timeout;
    lp->user_flag = lp0->user_flag;
    return fd; 
}

HOOK_SYS_FUNC_DEF(int, dup2, (int old, int newfd))
{

    HOOK_SYS_FUNC( dup2 );

    if( !conet::is_enable_sys_hook() )
    {   
        return _(dup2)(old, newfd);
    }   
    int fd = _(dup2)(old, newfd);
    if( fd < 0 ) 
    {   
        return fd; 
    }   
    fd_ctx_t *new_lp = get_fd_ctx(newfd);
    if (new_lp) {
        free_fd_ctx(newfd);
    }

    fd_ctx_t *lp0 = get_fd_ctx(old);

    if (!lp0) return fd; 

    fd_ctx_t *lp = alloc_fd_ctx( fd );
    lp->domain = lp0->domain;
    lp->rcv_timeout = lp0->rcv_timeout;
    lp->snd_timeout = lp0->snd_timeout;
    lp->user_flag = lp0->user_flag;

    return fd; 
}

HOOK_SYS_FUNC_DEF(int, dup3, (int old, int newfd, int flags))
{

    HOOK_SYS_FUNC( dup3 );

    if( !conet::is_enable_sys_hook() )
    {   
        return _(dup3)(old, newfd, flags);
    }   
    int fd = _(dup3)(old, newfd, flags);
    if( fd < 0 ) 
    {   
        return fd; 
    }   
    fd_ctx_t *new_lp = get_fd_ctx(newfd);
    if (new_lp) {
        free_fd_ctx(newfd);
    }

    fd_ctx_t *lp0 = get_fd_ctx(old);

    if (!lp0) return fd; 

    fd_ctx_t *lp = alloc_fd_ctx( fd );
    lp->domain = lp0->domain;
    lp->rcv_timeout = lp0->rcv_timeout;
    lp->snd_timeout = lp0->snd_timeout;
    lp->user_flag = lp0->user_flag;

    return fd; 
}




HOOK_SYS_FUNC_DEF(int,  select, 
        (int nfds, fd_set *readfds, fd_set *writefds,
        fd_set *exceptfds, struct timeval *timeout)
)
{
	HOOK_SYS_FUNC( select );
	if( !conet::is_enable_sys_hook() )
	{
		return _(select)(nfds, readfds, writefds, exceptfds, timeout);
	}

    std::vector<pollfd> pfs;
    for (int i=0; i<nfds; ++i){
        pollfd pf;
        pf.fd = -1;
        pf.events = 0;
        pf.revents = 0;
        if (readfds) {
            if (FD_ISSET(i, readfds)) {
                pf.fd = i;
                pf.events |= POLLIN;
            }
        }
        if (writefds) {
            if (FD_ISSET(i, writefds)) {
                pf.fd = i;
                pf.events |= POLLOUT;
            }
        }
        if (exceptfds) {
            if (FD_ISSET(i, exceptfds)) {
                pf.fd = i;
                pf.events |= POLLERR;
            }
        }
        if (pf.fd >=0) pfs.push_back(pf);
    }
	int to = 0;
    if (timeout) {
      to = ( timeout->tv_sec * 1000 ) 
				+ ( timeout->tv_usec / 1000 );
    }

    if (pfs.empty()) {
        return conet::co_poll(NULL, 0, to);
    }

    int ret = conet::co_poll(&pfs[0], pfs.size(), to);
    if (ret == 0) return 0;
    if (ret <0) return ret;

    if (readfds) {
        FD_ZERO(readfds);
    }
    if (writefds) {
        FD_ZERO(writefds);
    }
    if (exceptfds) {
        FD_ZERO(exceptfds);
    }

    int cnt = 0;
    int len = pfs.size();
    for (int i=0; i<len; ++i)
    {
        int set = 0;
        pollfd pf=pfs[i];
        if (readfds) {
            if (pf.revents & POLLIN) {
                FD_SET(pf.fd, readfds);
                set = 1; 
            }
        }
        if (writefds) {
            if (pf.revents & POLLOUT) {
                FD_SET(pf.fd, writefds);
                set = 1; 
            }
        }
        if (exceptfds) {
            if (pf.revents & POLLERR) {
                FD_SET(pf.fd, exceptfds);
                set = 1; 
            }
        }
        if (set) ++cnt;
    }
    return cnt;
}


HOOK_SYS_FUNC_DEF(int, pselect,(int nfds, fd_set *readfds, fd_set *writefds,
                       fd_set *exceptfds, const struct timespec *timeout,
                               const sigset_t *sigmask))
{
    HOOK_SYS_FUNC( pselect );
    if( !conet::is_enable_sys_hook() )
    {    
        return _(pselect)(nfds, readfds, writefds, exceptfds, timeout, sigmask);
    }    

    if (timeout) 
    {    
        struct timeval to;
        to.tv_sec  = timeout->tv_sec;
        to.tv_usec = timeout->tv_nsec/1000;
        return select(nfds, readfds, writefds, exceptfds, &to);
    } else {
        return select(nfds, readfds, writefds, exceptfds, NULL);
    }    
}

static
__thread conet::AresWrap *g_ares_wrap = NULL;

CONET_DEF_TLS_VAR_HELP_DEF(g_ares_wrap);

HOOK_SYS_FUNC_DEF(hostent *, gethostbyname, (char const *name))
{
    HOOK_SYS_FUNC(gethostbyname);
    if( !conet::is_enable_sys_hook() )
    {    
        return _(gethostbyname)(name);
    }
    
    return TLS_GET(g_ares_wrap)->gethostbyname(name);
}

HOOK_SYS_FUNC_DEF(hostent *, gethostbyname2, (char const *name, int af))
{
    HOOK_SYS_FUNC(gethostbyname2);
    if( !conet::is_enable_sys_hook() )
    {    
        return _(gethostbyname2)(name, af);
    }
    
    return TLS_GET(g_ares_wrap)->gethostbyname2(name, af);
}

}

namespace conet
{
    int my_accept4(int fd, struct sockaddr *addr, socklen_t *len, int flags)
    {
        if (!conet::is_enable_sys_hook())
        {
            return _(accept4)(fd, addr, len, flags);
        }

        int client_fd = -1;
        fd_ctx_t *lp = get_fd_ctx(fd);

        if (!lp || (O_NONBLOCK & lp->user_flag))
        {
            client_fd = _(accept4)(fd, addr, len, flags);
        }
        else
        {
            // block call
            struct pollfd pf = {fd : fd, events : POLLIN | POLLHUP | POLLERR};
            int ret = conet::co_poll(&pf, 1, -1);
            if (ret == 0)
            {
                errno = ETIMEDOUT;
                return -1;
            }
            if (ret < 0)
            {
                return -1;
            }
            if (pf.revents & POLLERR)
            {
                return -1;
            }
            if (pf.revents & POLLHUP)
            {
                return -1;
            }
            client_fd = _(accept4)(fd, addr, len, flags);
        }
        if (client_fd >= 0)
        {
            fd_ctx_t *ctx = NULL;
            if (flags & O_NONBLOCK)
            {
                ctx = conet::alloc_fd_ctx2(client_fd, 1, true);
            }
            else
            {
                ctx = conet::alloc_fd_ctx(client_fd, 1);
            }
            if (lp)
            {
                ctx->domain = lp->domain;
            }
        }
        return client_fd;
    }

    ssize_t poll_recv(int fd, void *buffer, size_t length, int timeout)
    {
        //fd_ctx_t *lp = get_fd_ctx(fd);
        struct pollfd pf = {0};
        pf.fd = fd;
        pf.events = (POLLIN | POLLERR | POLLHUP);
        int ret = conet::co_poll(&pf, 1, timeout);
        if (ret == 0) {
            errno = ETIMEDOUT;
            return -1;
        }

        if (ret < 0)
        {
            return -1;
        }

        if (pf.revents & POLLERR)
        {
            return -1;
        }

        if (pf.revents & POLLHUP)
        {
            return 0;
        }

        ret = _(recv)(fd, buffer, length, 0);
        return ret;
    }
}
