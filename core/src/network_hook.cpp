#include "coroutine.h"
#include "coroutine_impl.h"
#include "fd_ctx.h"
#include "log.h"

#include <sys/socket.h>
#include <sys/time.h>
#include <sys/syscall.h>
#include <sys/un.h>

#include <dlfcn.h>
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

#include "network_hook.h"
#include "hook_helper.h"

using conet::alloc_fd_ctx;
using conet::get_fd_ctx;
using conet::free_fd_ctx;
using conet::fd_ctx_t;


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

        struct pollfd pf = {
fd:
            fd,
events:
            POLLIN|POLLERR|POLLHUP
        };
        poll( &pf,1, -1);
        client_fd =  _(accept)(fd, addr, len);
    }
    if (client_fd >=0) {
        fd_ctx_t *ctx = alloc_fd_ctx(client_fd);
        ctx->domain = lp->domain;
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

    fd_ctx_t *lp = get_fd_ctx( fd );

    if( !lp || ( O_NONBLOCK & lp->user_flag ) )
    {
        return  _(accept4)(fd, addr, len, flags);
    }

    //block call

    struct pollfd pf = {
fd:
        fd,
events:
        POLLIN|POLLERR|POLLHUP
    };
    poll( &pf,1, -1);

    return _(accept4)(fd, addr, len, flags);
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

    struct pollfd pf = {
fd:
        fd,
events:
        POLLOUT|POLLERR|POLLHUP
    };

    int poll_ret = poll ( &pf,1, lp->snd_timeout);
    if (0 == poll_ret) {
        // timeout
        errno = ETIMEDOUT;
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
    HOOK_SYS_FUNC(close);
    if( !conet::is_enable_sys_hook() )
    {
        return SYS_FUNC(close)( fd );
    }

    int ret = SYS_FUNC(close)(fd);
    free_fd_ctx( fd );
    return ret;
}

HOOK_SYS_FUNC_DEF(
    ssize_t, read, ( int fd, void *buf, size_t nbyte )
)
{

    HOOK_SYS_FUNC(read);
    if( !conet::is_enable_sys_hook() ) {
        return _(read)(fd, buf, nbyte );
    }

    fd_ctx_t *lp = get_fd_ctx( fd );
    ssize_t ret = 0;

    if( !lp || ( O_NONBLOCK & lp->user_flag ) )
    {
        ret = _(read)(fd, buf, nbyte);
        return ret;
    }

    int timeout = lp->rcv_timeout;

    struct pollfd pf = {
fd:
        fd,
events:
        POLLIN | POLLERR | POLLHUP
    };

    poll( &pf, 1, timeout );

    ret = _(read)( fd,(char*)buf , nbyte );
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

    fd_ctx_t *lp = get_fd_ctx( fd );

    if( !lp || ( O_NONBLOCK & lp->user_flag ) )
    {
        ssize_t ret = _(write)( fd,buf,nbyte );
        return ret;
    }

    ssize_t ret = 0;
    ret = _(write)(fd, (const char*) buf, nbyte);
    if (ret >=0) {
        return ret;
    }

    if (errno != EAGAIN) return ret;

    int timeout = lp->snd_timeout;
    struct pollfd pf = {
fd :
        fd,
events:
        ( POLLOUT | POLLERR | POLLHUP )
    };
    poll( &pf,1,timeout );
    ret = _(write)(fd, (const char*)buf, nbyte);
    return ret;
}

HOOK_SYS_FUNC_DEF(
    ssize_t, sendto, (int fd, const void *message, size_t length,
                      int flags, const struct sockaddr *dest_addr,
                      socklen_t dest_len)
)
{
    HOOK_SYS_FUNC(sendto);
    if( !conet::is_enable_sys_hook() )
    {
        return _(sendto)(fd, message, length, flags, dest_addr, dest_len);
    }

    fd_ctx_t *lp = get_fd_ctx(fd);
    if( !lp || ( O_NONBLOCK & lp->user_flag ) )
    {
        return _(sendto)(fd,message,length,flags,dest_addr,dest_len );
    }

    ssize_t ret = _(sendto)(fd, message, length, flags, dest_addr, dest_len);
    if( ret < 0 && EAGAIN == errno )
    {
        int timeout = lp->snd_timeout;

struct pollfd pf = {fd: fd, events: ( POLLOUT | POLLERR | POLLHUP ) };
        poll(&pf, 1, timeout);
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
    if( !lp || ( O_NONBLOCK & lp->user_flag ) )
    {
        return _(recvfrom)(fd, buffer, length, flags, address, address_len);
    }

    int timeout = lp->rcv_timeout;

struct pollfd pf = { fd:fd, events:( POLLIN | POLLERR | POLLHUP ) };
    poll( &pf,1,timeout );
    ssize_t ret = _(recvfrom)(fd, buffer, length, flags, address, address_len);
    return ret;
}

HOOK_SYS_FUNC_DEF(
    ssize_t, send, (int fd, const void *buffer, size_t length, int flags)
)
{

    HOOK_SYS_FUNC(send);
    if( !conet::is_enable_sys_hook() )
    {
        return _(send)(fd, buffer, length, flags);
    }
    fd_ctx_t *lp = get_fd_ctx(fd);

    if( !lp || ( O_NONBLOCK & lp->user_flag ) )
    {
        return _(send)(fd, buffer, length, flags);
    }
    int timeout = lp->snd_timeout;

    ssize_t ret = _(send)(fd, buffer, length, flags);
    if (ret == -1 && errno == EAGAIN) {
        struct pollfd pf = { 0 };
        pf.fd = fd;
        pf.events = ( POLLOUT | POLLERR | POLLHUP );
        poll( &pf,1,timeout );

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

    if( !lp || ( O_NONBLOCK & lp->user_flag ) )
    {
        return _(recv)(fd, buffer, length, flags);
    }
    int timeout = lp->rcv_timeout;

    struct pollfd pf = { 0 };
    pf.fd = fd;
    pf.events = ( POLLIN | POLLERR | POLLHUP );
    poll( &pf,1, timeout );

    ssize_t ret = _(recv)(fd, buffer, length, flags);
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
        ret = _(fcntl)( fd,cmd,param );
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
        ret = _(fcntl)( fd,cmd );
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

HOOK_SYS_FUNC_DEF(int, usleep, (useconds_t us))
{

    HOOK_SYS_FUNC(usleep);
    if( !conet::is_enable_sys_hook() )
    {
        return _(usleep)(us);
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



HOOK_SYS_FUNC_DEF(unsigned int, sleep, (unsigned int s))
{
    HOOK_SYS_FUNC(sleep);
    if( !conet::is_enable_sys_hook() )
    {
        return _(sleep)(s);
    }
    return conet::co_poll(NULL, 0, s*1000);
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
        return poll(NULL, 0, to);
    }

    int ret = poll(&pfs[0], pfs.size(), to);
    if (ret == 0) return 0;

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

namespace conet
{

int is_enable_sys_hook()
{
    return current_coroutine()->is_enable_sys_hook;
}

void enable_sys_hook()
{
    current_coroutine()->is_enable_sys_hook = 1;
}

void disable_sys_hook()
{
    current_coroutine()->is_enable_sys_hook = 0;
}

}

