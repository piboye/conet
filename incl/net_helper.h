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
#ifndef __NET_HELPER_H_INC__
#define __NET_HELPER_H_INC__

#include <stdio.h>
#include <stdlib.h>
#include <stdint.h>
#include <stack>

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

namespace net_helper
{

static inline
uint64_t rdtscp(void)
{
	register uint32_t lo, hi;
	register uint64_t o;
    __asm__ __volatile__ (
        "rdtscp" : "=a"(lo), "=d"(hi)
        );
	o = hi;
	o <<= 32;
	return (o | lo);
}

static inline
uint64_t get_cpu_khz() 
{
	FILE *fp = fopen("/proc/cpuinfo","r");
	if(!fp) return 1;
	char buf[4096] = {0};
	fread(buf,1,sizeof(buf),fp);
	fclose(fp);

	char *lp = strstr(buf,"cpu MHz");
	if(!lp) return 1;
	lp += strlen("cpu MHz");
	while(*lp == ' ' || *lp == '\t' || *lp == ':')
	{
		++lp;
	}

	double mhz = atof(lp);
	uint64_t u = (uint64_t)(mhz * 1000);
	return u;
}

static inline 
uint64_t get_now_ms() 
{
    struct timeval te;
    gettimeofday(&te, NULL);
    uint64_t ms = te.tv_sec*1000LL + te.tv_usec/1000;
    return ms;
}

static inline 
uint64_t get_tick_ms()
{
    //return get_now_ms();
	static uint64_t khz = get_cpu_khz();
	return rdtscp() / khz;
}


enum {
    ERR_TIMEOUT_NET_HELPER = -2  // 超时错误码
};


#define NET_HELPER_DECLAR_TIMETOUT() \
    int rest_timeout = timeout; \
    int used_timeout = 0; \
    int use_timeout = rest_timeout\

#define NET_HELPER_UPDATE_TIMEOUT() \
    do { \
        rest_timeout -= use_timeout; \
        used_timeout += use_timeout; \
        if (cost_timeout) *cost_timeout = used_timeout; \
        if (rest_timeout < 0) { \
            return ERR_TIMEOUT_NET_HELPER; \
        } \
    } while(0)


static inline 
int send_data(int fd, char const * buf, size_t len, int timeout=10, int * cost_timeout=NULL) 
{
    int ret = 0;
    size_t cur_len = 0;
    int rest_timeout = timeout;
    uint64_t start = get_tick_ms();
    uint64_t now = start;
    while (1)
    {
        ret = ::write(fd, &buf[cur_len], len-cur_len);
        if (ret <0) {
            if ( errno == EAGAIN || errno == EWOULDBLOCK) {
                rest_timeout = timeout - (now -start);
                if (rest_timeout <=0) {
                    if (cost_timeout) *cost_timeout = (now-start); 
                    return ERR_TIMEOUT_NET_HELPER;
                }
                struct pollfd pf = { 0 };
                pf.fd = fd;
                pf.events = (POLLOUT|POLLERR|POLLHUP);
                poll(&pf, 1, rest_timeout);
                now = get_tick_ms();
            } else {
                if (cost_timeout) *cost_timeout = (now-start); 
                return -__LINE__;
            }
        }
        else if (ret == 0) {
            if (cost_timeout) *cost_timeout = (now-start); 
            return 0;
        } else{
            cur_len += ret;
        }
        if (cur_len >= len) break;
    }
    if (cost_timeout) *cost_timeout = (now-start); 
    return len;
}



static inline
int send_data_pack(int fd, char const *buf, size_t a_len, int timeout=10, int *cost_timeout=NULL)
{
    int ret = 0;

    uint32_t len = 0;
    
    len = htonl(a_len);

    char *out =new char[a_len+sizeof(len)];
    memcpy(out, (char *)&len, sizeof(len));
    memcpy(out+sizeof(len), buf, a_len);
    ret = send_data(fd, out, a_len +sizeof(len), timeout, cost_timeout);
    if (ret > 4) ret -=4;
    delete out;
    return ret;
}

static inline
int send_data_pack(int fd, std::string data, int timeout=10, int *cost_timeout=NULL)
{
    uint32_t len = 0;
    len = htonl(data.size());
    data.insert(0, (char *)(&len), sizeof(len));

    int ret = send_data(fd, data.c_str(), data.size(), timeout, cost_timeout);
    if (ret > 4) ret -=4 ;
    return ret;
}

static inline
int send_data_pack(int fd, std::vector<char> const & data, int timeout=10, int *cost_timeout=NULL)
{
    return send_data_pack(fd, &data[0], data.size(), timeout, cost_timeout);
}

static inline
int read_data(int fd, char *buff, size_t len, int timeout, int *cost_timeout=NULL) 
{
    int ret = 0;
    size_t cur_len =0;
    int rest_timeout = timeout;
    uint64_t start = get_tick_ms();
    uint64_t now = start;
    while (1)
    {
        ret = read(fd, &buff[cur_len], len-cur_len);
        if (ret < 0) {
            if ( errno == EAGAIN || errno == EWOULDBLOCK) {
                rest_timeout = timeout - (now -start);
                if (rest_timeout <=0){
                    if (cost_timeout) *cost_timeout = (now-start); 
                    return ERR_TIMEOUT_NET_HELPER;
                }  
                struct pollfd pf = { 0 };
                pf.fd = fd;
                pf.events = (POLLIN|POLLERR|POLLHUP);
                poll(&pf, 1, rest_timeout);
                now = get_tick_ms();
            } else {
                if (cost_timeout) *cost_timeout = (now-start); 
                return -__LINE__;
            }
        }
        else if (ret == 0) {
            if (cost_timeout) *cost_timeout = (now-start); 
            return 0;
        } else {
            cur_len += ret;
        }
        if (cur_len >= len) break;
    }
    if (cost_timeout) *cost_timeout = (now-start); 
    return len;
}

static
inline
int read_one_pack(int fd, std::string *result, 
            int timeout = 10,  
            int max_len = 1024*1024, 
            int *cost_timeout = NULL
            ) 
{
    NET_HELPER_DECLAR_TIMETOUT();

    uint32_t len = 0;
    int ret = 0;
    ret = read_data(fd, (char *)&len, sizeof(len), rest_timeout, &use_timeout);
    NET_HELPER_UPDATE_TIMEOUT();
    if (ret != (int) sizeof(len)) {
        return -__LINE__;
    }

    len = ntohl(len);
    if ((int64_t) len >  max_len) return 0;

    result->resize(len, '0');
    ret = read_data(fd, (char *)result->data(), len, rest_timeout, &use_timeout);
    if (ret >0) result->resize(ret);
    else result->clear();
    NET_HELPER_UPDATE_TIMEOUT();

    return ret; 
}

static inline 
int set_none_block(int fd)
{
    int ret =0;
    int flags=0;

    flags = fcntl(fd, F_GETFL, 0);
    flags |= O_NONBLOCK;
    flags |= O_NDELAY;
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
int create_tcp_socket(int port = 0, const char *ip_txt  = "*", int reuse = true)
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
int connect_to(char const *ip_txt, int port, int timeout=10, const char *client_ip=NULL, int client_port=0)
{
    int fd = create_tcp_socket(client_port, client_ip);
    struct sockaddr_in addr;
    set_addr(&addr, ip_txt, port);
    int ret = 0;
    set_none_block( fd );
    ret = connect(fd, (struct sockaddr*)&addr,sizeof(addr)); 
    if (ret<0) {
        if (errno == EINPROGRESS) {
            struct pollfd pf = { 0 };
            pf.fd = fd;
            pf.events = (POLLIN|POLLOUT|POLLERR|POLLHUP);
            poll(&pf, 1, timeout);
            ret = connect(fd, (struct sockaddr*)&addr,sizeof(addr)); 
            if (ret) {
                close(fd);
                return -1;
            }
            return fd;
        } else {
            close(fd);
            return -1;   
        }
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


} // net_helper namespace end

#endif 

