/*
 * =====================================================================================
 *
 *       Filename:  server_base.h
 *
 *    Description:
 *
 *        Version:  1.0
 *        Created:  2014年05月11日 07时43分57秒
 *       Revision:  none
 *       Compiler:  gcc
 *
 *         Author:  piboyeliu
 *   Organization:
 *
 * =====================================================================================
 */
#ifndef __SERVER_BASE_H_INC__
#define __SERVER_BASE_H_INC__
#include <string>
#include <stdint.h>
#include <netinet/in.h>
#include "conet_all.h"
#include "list.h"
#include "co_pool.h"

namespace conet
{


struct coroutine_t;
struct conn_info_t;
struct server_t
{
    int listen_fd;
    std::string ip;
    int port;
    coroutine_t *co;
    int state;
    int max_packet_size;
    int (*proc)(conn_info_t *conn);
    co_pool_t co_pool;
    void *extend;
};

struct conn_info_t
{
    server_t * server;
    uint32_t ip;
    int port;
    int fd;
    struct sockaddr_in addr;
    coroutine_t *co;
};

int init_server(server_t *server, const char *ip, int port, int max_packet_size=102400);
int start_server(server_t *server);


}

#endif /* end of include guard */

