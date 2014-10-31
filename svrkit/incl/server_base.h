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
#include "co_pool.h"
#include "base/incl/list.h"
#include "base/incl/obj_pool.h"

namespace conet
{

struct server_t;
struct conn_info_t
{
    server_t * server;
    uint32_t ip;
    int port;
    int fd;
    struct sockaddr_in addr;
    coroutine_t *co;
    void *extend;
    conn_info_t()
    {
    }
};

struct coroutine_t;
struct server_t
{
    enum {
        SERVER_START=0,
        SERVER_RUNNING=1,
        SERVER_STOPED=2,
    };

    int listen_fd;
    std::string ip;
    int port;

    coroutine_t *main_co;
    int state;
    int to_stop;
    int (*proc)(conn_info_t *conn);

    obj_pool_t co_pool;

    ObjPool<conn_info_t> conn_info_pool;
    void *extend;
    struct server_base_conf_t 
    {
        int listen_backlog;
        int max_packet_size;
        int max_conn_num;
    } conf;

    struct server_base_data_t
    {
        int cur_conn_num;
    } data;
};


int init_server(server_t *server, const char *ip, int port);
int start_server(server_t *server);
int stop_server(server_t *server, int wait_ms=0);


}

#endif /* end of include guard */

