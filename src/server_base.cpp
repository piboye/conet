/*
 * =====================================================================================
 *
 *       Filename:  server_base.cpp
 *
 *    Description:  
 *
 *        Version:  1.0
 *        Created:  2014年05月10日 15时55分50秒
 *       Revision:  none
 *       Compiler:  gcc
 *
 *         Author:  YOUR NAME (), 
 *   Organization:  
 *
 * =====================================================================================
 */
#include <stdlib.h>
#include <stdint.h>
#include <string>
#include <netinet/tcp.h>
#include "coroutine.h"
#include "net_helper.h"
#include "server_base.h"

using namespace net_helper;

namespace conet
{

int init_server(server_t *server, const char *ip, int port, int max_packet_size)
{
    server->ip = ip;
    server->port = port;
    server->state = 0;
    server->co = NULL;
    server->max_packet_size = max_packet_size;
    return 0;
}

int server_main(void *arg);

int start_server(server_t *server)
{
     server->co = alloc_coroutine(server_main, server); 
     conet::resume(server->co);
     return 0;
}

 

int server_main(void *arg)
{
    server_t *server = (server_t *)(arg);

    conet::enable_sys_hook();

    int listen_fd = net_helper::create_tcp_socket(server->port, server->ip.c_str());
    listen(listen_fd, 1024); 
    set_none_block(listen_fd);

    server->listen_fd = listen_fd;
    int waits = 5; // 5 seconds;
    setsockopt(listen_fd, IPPROTO_IP, TCP_DEFER_ACCEPT, &waits, sizeof(waits));

    int ret = 0;
    while (server->state == 0) {
        struct pollfd pf = { 0 };
        pf.fd = listen_fd;
        pf.events = (POLLIN|POLLERR|POLLHUP);
        ret = poll(&pf, 1, 1000);
        if (ret <=0) continue; 

        struct sockaddr_in addr; //maybe sockaddr_un;
        memset( &addr,0,sizeof(addr) );
        socklen_t len = sizeof(addr);

        int fd = accept(listen_fd, (struct sockaddr *)&addr, &len);

        if (fd <0) continue;

        conn_info_t *conn_info = new conn_info_t();
        conn_info->server = server;
        memcpy(&conn_info->addr, &addr,len);
        conn_info->fd = fd;

        conn_info->co = alloc_coroutine((int (*)(void *))server->proc, conn_info);
        //set_auto_delete(conn_info->co);
        conet::resume(conn_info->co);
    }
    return 0;
}

}
