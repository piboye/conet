/*
 * =====================================================================================
 *
 *       Filename:  net_tool.cpp
 *
 *    Description
 *
 *        Version:  1.0
 *        Created:  2014年07月23日 15时49分59秒
 *       Revision:  none
 *       Compiler:  gcc
 *
 *         Author:  piboyeliu
 *   Organization:  
 *
 * =====================================================================================
 */
#include "net_tool.h"

#include <stack>

#include <sys/syscall.h>
#include <sys/types.h>
#include <sys/time.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <sys/un.h>
#include <fcntl.h>
#include <errno.h>
#include <poll.h>
#include <sys/ioctl.h>
#include <linux/netdevice.h>
#include <netinet/tcp.h>

#include <vector>
#include <string>

#include "time_helper.h"
#include "glog/logging.h"

namespace conet 
{

ssize_t write_timeout(int fd, const void *buf, size_t nbyte, int timeout)
{

    ssize_t ret = 0;

    //ret = syscall(SYS_write, fd, (const char*) buf, nbyte);
    //ret = write(fd, (const char*) buf, nbyte);
    ret = send(fd, (const char*) buf, nbyte, MSG_DONTWAIT);
    if (ret >=0) {
        return ret;
    }

    if (errno != EAGAIN) return ret;

    struct pollfd pf = { fd : fd, events: ( POLLOUT | POLLERR | POLLHUP ) };
    ret =  poll( &pf, 1, timeout );
    if (ret == 0) {
        // timeout;
        return -2;
    }
    if (ret <0) {
        return -1;
    }
    if (pf.revents & POLLERR) {
        return -1;
    }
    if (!(pf.revents &POLLOUT))
    {
        LOG(ERROR)<<"poll write failed, [events:"<<pf.revents<<"]";
        return -1;
    }
    //ret = syscall(SYS_write, fd, (const char*)buf, nbyte);
    ret = send(fd, (const char*)buf, nbyte, MSG_DONTWAIT);
    return ret;
}

int send_data(int fd, char const * buf, size_t len, int timeout)
{
    int ret = 0;
    size_t cur_len = 0;
    
    uint64_t start_ms = conet::get_tick_ms();
    uint64_t cur = start_ms;
    int rest_ms = timeout;

    while (cur_len < len)
    {
        ret = conet::write_timeout(fd, &buf[cur_len], len-cur_len, rest_ms);
        if (ret <0) {
            break;
        }
        else if (ret == 0) {
            break;
        }

        cur_len += ret;

        cur = conet::get_tick_ms();

        rest_ms = timeout - (int)(cur -start_ms); 

        if (rest_ms <=0) {
            return ERR_TIMEOUT_NET_HELPER;
        }
    }

    if (ret <0) return ret;
    return len;
}

ssize_t read_timeout(int fd, void *buf, size_t nbyte, int timeout, int has_data=0)
{



    ssize_t ret = 0;
    if (has_data) {
        ret = recv(fd,(char*)buf , nbyte , MSG_DONTWAIT);
        if (ret >=0) {
            return ret;
        }

        if (errno != EAGAIN) return ret;
    }

    struct pollfd pf = {
        fd: fd,
        events: POLLIN | POLLERR | POLLHUP
    };

    ret = poll( &pf, 1, timeout );
    if (ret == 0) {
        return -2;
    }

    if (ret <0) { // poll may error when fd reset by peer;
        return -1;
    }


    if (pf.revents & POLLERR) {
        return -1;
    }
    if (!(pf.revents & POLLIN))
    {
        LOG(ERROR)<<"poll read failed, [events:"<<pf.revents<<"]";
        return -1;
    }

    ret = recv(fd,(char*)buf , nbyte , MSG_DONTWAIT);
    return ret;
}

int read_data(int fd, char *buff, size_t len, int timeout)
{
    uint64_t start_ms = conet::get_tick_ms();
    uint64_t cur = start_ms;
    int rest_ms = timeout;

    int ret = 0;
    size_t cur_len =0;
    while (cur_len < len)
    {
        ret = read_timeout(fd, &buff[cur_len], len-cur_len, rest_ms);
        if (ret <= 0) {
            return ret;
        }
        cur_len += ret;

        cur = conet::get_tick_ms();

        rest_ms = timeout - (int)(cur -start_ms); 

        if (rest_ms <=0) {
            return ERR_TIMEOUT_NET_HELPER;
        }
    }
    return cur_len;
}

int read_data(int fd, char *buff, size_t len)
{
    int ret = 0;
    size_t cur_len =0;
    while (cur_len < len)
    {
        ret = read(fd, &buff[cur_len], len-cur_len);
        if (ret <= 0) {
            return ret;
        }
        cur_len += ret;
    }
    return cur_len;
}

int set_nodelay(int fd, bool enable)
{
   int flag =  enable ? 1: 0;
   return setsockopt(fd , IPPROTO_TCP, TCP_NODELAY, (char *)&flag, sizeof(flag));
}

int set_none_block(int fd, bool enable)
{
    int ret =0;
    int flags=0;

    flags = fcntl(fd, F_GETFL, 0);
    if ( enable)  {
        flags |= O_NONBLOCK;
        flags |= O_NDELAY;
    } else {
        flags &= ~O_NONBLOCK;
        flags &= ~O_NDELAY;
    }
    ret = fcntl(fd, F_SETFL, flags);

    return ret;
}

void set_addr(struct sockaddr_in *addr, const char *ip_txt,const unsigned short port)
{
    bzero(addr,sizeof(*addr));
    addr->sin_family = AF_INET;
    addr->sin_port = htons(port);
    int ip = 0;
    if( !ip_txt
            || '\0' == *ip_txt
            || 0 == strcmp(ip_txt,"0")
            || 0 == strcmp(ip_txt,"0.0.0.0")
            || 0 == strcmp(ip_txt,"*")
      )
    {
        ip = htonl(INADDR_ANY);
    }
    else
    {
        ip = inet_addr(ip_txt);
    }
    addr->sin_addr.s_addr = ip;
}


static int g_can_reuse_port = can_reuse_port();

#ifndef SO_REUSEPORT
#define SO_REUSEPORT 15
#endif 

int can_reuse_port() 
{
   static int init = 0;
   if (0 == init) {
        int fd = socket(AF_INET,SOCK_STREAM, IPPROTO_TCP);
        int reuse_addr = 1;
        int ret = setsockopt(fd, SOL_SOCKET, SO_REUSEPORT, &reuse_addr,sizeof(reuse_addr));
        if (ret) {
            g_can_reuse_port = 0; 
        } else {
            g_can_reuse_port = 1;
        }
        close(fd);
        init = 1;
   }
   return g_can_reuse_port;
}

int create_tcp_socket(int port, const char *ip_txt, int reuse)
{
    int fd = socket(AF_INET,SOCK_STREAM, IPPROTO_TCP);
    if( fd >= 0 ) {
        if(port != 0) {
            if(reuse) {
                int reuse_addr = 1;
                if (g_can_reuse_port) {
                    setsockopt(fd, SOL_SOCKET, SO_REUSEPORT, &reuse_addr,sizeof(reuse_addr));
                } else {
                    setsockopt(fd, SOL_SOCKET, SO_REUSEADDR, &reuse_addr,sizeof(reuse_addr));
                }
            }
            struct sockaddr_in addr ;
            set_addr(&addr, ip_txt, port);
            int ret = bind(fd,(struct sockaddr*)&addr,sizeof(addr));
            if( ret != 0) {
                close(fd);
                return -1;
            }
        }
    }
    return fd;
}

int connect_to(char const *ip_txt, int port, 
        const char *client_ip, int client_port)
{
    int fd = create_tcp_socket(client_port, client_ip);
    struct sockaddr_in addr;
    set_addr(&addr, ip_txt, port);
    int ret = 0;
    ret = connect(fd, (struct sockaddr*)&addr,sizeof(addr));
    if (ret) {
        close(fd);
        return -1;
    }
    return fd;
}

const char* get_local_ip(const char* pIfConf)
{
    int32_t sockfd = socket(AF_INET, SOCK_DGRAM, 0);
    if( sockfd < 0 )
    {
        close(sockfd);
        return NULL;
    }

    //初始化ifconf
    char szBuf[512];
    struct ifconf stIfConf;
    stIfConf.ifc_len = 512;
    stIfConf.ifc_buf = szBuf;

    //获取所有接口信息
    ioctl(sockfd, SIOCGIFCONF, &stIfConf);

    //接下来一个一个的获取IP地址
    struct ifreq* pstIfreq = (struct ifreq*)szBuf;
    for (int32_t nIter=(stIfConf.ifc_len/sizeof(struct ifreq)); nIter>0; nIter--)
    {
        if( strcmp(pIfConf,pstIfreq->ifr_name) == 0 )
        {
            close(sockfd);
            return inet_ntoa(((struct sockaddr_in*)&(pstIfreq->ifr_addr))->sin_addr);
        }
        pstIfreq++;
    }

    close(sockfd);
    return NULL;
}

int PacketStream::read_packet(char **pack, int * pack_len, int timeout, int a_has_data) 
{
    uint32_t len = 0;
    int ret = 0;
    int cur_len = 0;

    if (prev_pos >0) {
        cur_len = total_len - prev_pos;
        memmove(buff, buff+prev_pos, cur_len);
    }

    uint64_t start_ms = conet::get_tick_ms();
    uint64_t cur = start_ms;
    int rest_ms = timeout;

    int has_data = a_has_data;
    while (cur_len < (int)sizeof(len)) {
        ret = read_timeout(fd, buff+cur_len, max_size-cur_len, rest_ms, has_data);
        if (ret <= 0) {
            return ret;
        } 
        cur_len += ret;

        has_data = 0;

        cur = conet::get_tick_ms();
        rest_ms = timeout - (int)(cur -start_ms); 
        if (rest_ms <=0) {
            return ERR_TIMEOUT_NET_HELPER;
        }
    }

    if (isalpha(buff[0])) {
        prev_pos = cur_len;
        return HTTP_PROTOCOL_DATA;
    }


    len = ntohl(*(uint32_t *)(buff));

    if ((int32_t) len <=0) return -1;
    if ((int32_t) len + 4 >  max_size) return -4;

    while (cur_len - 4 < (int)len) {
        ret = read_timeout(fd, buff+cur_len, max_size-cur_len, rest_ms);
        if (ret <= 0) {
            return ret;
        } 
        cur_len += ret;

        cur = conet::get_tick_ms();
        rest_ms = timeout - (int)(cur -start_ms); 
        if (rest_ms <=0) {
            return ERR_TIMEOUT_NET_HELPER;
        }
    }

    *pack_len = len;
    total_len = cur_len;

    prev_pos = len + 4;

    *pack = buff + 4;

    return 1; 
}

int PacketStream::read_packet(char **pack, int * pack_len) 
{
    uint32_t len = 0;
    int ret = 0;
    int cur_len = 0;
    
    if (prev_pos >0) {
        cur_len = total_len - prev_pos;
        memmove(buff, buff+prev_pos, cur_len);
    }

    while (cur_len < (int) sizeof(len)) {
        ret = read(fd, buff+cur_len, max_size-cur_len);
        if (ret <= 0) {
            return ret;
        } 
        cur_len += ret;
    }

    if (isalpha(buff[0])) {
        prev_pos = cur_len;
        return HTTP_PROTOCOL_DATA;
    }

    len = ntohl(*(uint32_t *)(buff));

    if ((int32_t) len <=0) return -1;

    if ((int32_t) len + 4 >  max_size) return -4;

    while (cur_len - 4 < (int) len) {
        ret = read(fd, buff+cur_len, max_size-cur_len);
        if (ret <= 0) {
            return ret;
        } 
        cur_len += ret;
    }

    *pack_len = len;
    total_len = cur_len;

    prev_pos = len + 4;

    *pack = buff + 4;

    return 1; 
}

}
