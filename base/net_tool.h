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
#include <unistd.h>
#include <string.h>
#include <arpa/inet.h>
#include <malloc.h>
#include <vector>

 extern "C"
{
    int enable_reuseport_cbpf(int fd);
}

namespace conet
{

enum
{
    ERR_TIMEOUT_NET_HELPER = -2  // 超时错误码
};


int send_data(int fd, char const * buf, size_t len, int timeout=100);

template <typename T>
int send_pb_obj(int fd,  T const &data, char *buf, int max_size, int timeout=1000)
{
    uint32_t len = data.ByteSize();

    *((uint32_t *)buf) = htonl(len);
    data.SerializeToArray(buf+4, len);

    return send_data(fd, buf, len + 4, timeout);
}


int read_data(int fd, char *buff, size_t len);
int read_data(int fd, char *buff, size_t len, int timeout);

class PacketStream
{
public:
    enum {
        HTTP_PROTOCOL_DATA=-10000,
    };
    int fd;
    int max_size;
    char *buff;
    int prev_pos;
    int total_len;

    bool is_http;

    explicit
    PacketStream(int max_size) 
    {
        fd = -1;
        this->max_size = max_size;
        buff = (char *)memalign(64, max_size); // 64 byte is cpu cache_line size
        prev_pos = 0;
        total_len = 0;
        is_http = false;
    }


    int init(int fd)
    {
        this->fd = fd;
        prev_pos = 0;
        total_len = 0;
        return 0;
    }



    int read_packet(char **pack, int * pack_len, int timeout, int has_data=0); 
    int read_packet(char **pack, int * pack_len);

    // 获取当前缓存的下一个包
    int next_packet(char **pack, int * pack_len);

    ~PacketStream()
    {
        if (buff) free(buff);
    }

};

int write_all(int fd, std::vector<std::vector<char>*> const &out_datas);

int can_reuse_port();

int set_none_block(int fd, bool enable=true);
int set_nodelay(int fd, bool enable=true);

void set_addr(struct sockaddr_in *addr, const char *ip_txt,const unsigned short port);

int create_tcp_socket(int port = 0, const char *ip_txt  = "*", int reuse = false);
int create_udp_socket(int port = 0, const char *ip_txt  = "*", int reuse = false);

int connect_to(char const *ip_txt, int port, const char *client_ip=NULL, int client_port=0);

const char* get_local_ip(const char* pIfConf="eth1");

}

#endif

