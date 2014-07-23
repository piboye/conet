/*
 * =====================================================================================
 *
 *       Filename:  net_helper.h
 *
 *    Description:
 *
 *        Version:  1.0
 *        Created:  04/19/2014 02:36:41 PM
 *       Revision:  none
 *       Compiler:  gcc
 *
 *         Author:  piboyeliu
 *   Organization:
 *
 * =====================================================================================
 */
#ifndef __NET_TOOL_H_INC__
#define __NET_TOOL_H_INC__

#include <stdio.h>
#include <stdlib.h>
#include <stdint.h>
#include <stack>

#include <unistd.h>
#include <sys/time.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <sys/un.h>
#include <fcntl.h>
#include <arpa/inet.h>
#include <errno.h>
#include <poll.h>
#include <sys/ioctl.h>
#include <linux/netdevice.h>

#include <string>
#include <vector>

#include "time_helper.h"
#include "log.h"

enum {
    ERR_TIMEOUT_NET_HELPER = -2  // 超时错误码
};


static inline
int send_data(int fd, char const * buf, size_t len)
{
    int ret = 0;
    size_t cur_len = 0;
    while (cur_len < len)
    {
        ret = ::write(fd, &buf[cur_len], len-cur_len);
        if (ret <0) {
            break;
        }
        else if (ret == 0) {
            break;
        }
        cur_len += ret;
    }

    if (ret <0) return ret;
    return len;
}

template <typename T>
int send_pb_obj(int fd,  T const &data, std::vector<char> *buf)
{
    uint32_t len = data.ByteSize();
    
    buf->resize(len+4);
    char * p = buf->data();
    *((uint32_t *)p) = htonl(len);
    data.SerializeToArray(p+4, len);

    return send_data(fd, p, buf->size());
}


static inline
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


class PacketStream
{
public:
    int fd;
    int max_size;
    char *buff;
    int prev_pos;
    int total_len;

    int init(int fd, int max_size) 
    {
        this->fd = fd;
        this->max_size = max_size;
        buff = (char *)malloc(max_size);
        prev_pos = 0;
        total_len = 0;
        return 0;
    }

    ~PacketStream()
    {
        free(buff);
    }

    int read_packet(char **pack, int * pack_len) 
    {
        uint32_t len = 0;
        int ret = 0;
        int cur_len = 0;
        
        if (prev_pos >0) {
            cur_len = total_len - prev_pos;
            memmove(buff, buff+prev_pos, cur_len);
        }

        while (cur_len < (int)sizeof(len)) {
            ret = read(fd, buff+cur_len, max_size-cur_len);
            if (ret <= 0) {
                return ret;
            } 
            cur_len += ret;
        }


        len = ntohl(*(uint32_t *)(buff));

        if ((int32_t) len + 4 >  max_size) return -4;

        while (cur_len - 4 < (int)len) {
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
};


static inline
int set_none_block(int fd, bool enable=true)
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

static inline
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

static inline
int create_tcp_socket(int port = 0, const char *ip_txt  = "*", int reuse = false)
{
    int fd = socket(AF_INET,SOCK_STREAM, IPPROTO_TCP);
    if( fd >= 0 ) {
        if(port != 0) {
            if(reuse) {
                int reuse_addr = 1;
                setsockopt(fd, SOL_SOCKET, SO_REUSEADDR, &reuse_addr,sizeof(reuse_addr));
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

static inline
int connect_to(char const *ip_txt, int port, const char *client_ip=NULL, int client_port=0)
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

static
inline
const char* get_local_ip(const char* pIfConf="eth1")
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


#endif

