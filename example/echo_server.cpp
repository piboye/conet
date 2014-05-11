/*
 * =====================================================================================
 *
 *       Filename:  echo_server.cpp
 *
 *    Description:  
 *
 *        Version:  1.0
 *        Created:  2014年05月11日 07时50分16秒
 *       Revision:  none
 *       Compiler:  gcc
 *
 *         Author:  YOUR NAME (), 
 *   Organization:  
 *
 * =====================================================================================
 */
#include <stdlib.h>
#include <stdio.h>
#include "server_base.h"
#include <errno.h>
#include <string.h>

using namespace conet;


inline 
int proc_echo(conn_info_t *conn)
{
    conet::enable_sys_hook();
    server_t * server= conn->server; 
    int size = server->max_packet_size;
    char * buff = (char *)malloc(size);
    int ret = 0;
    do
    {
        ret = ::read(conn->fd,  buff, size);
        if (ret <=0) {
            break;
        }

        ret = ::write(conn->fd, buff, ret);
        if (ret <=0) break; 
    } while(1);
    free(buff);
    close(conn->fd);
    delete conn;
    return 0;
}

int main(int argc, char const* argv[])
{
    if (argc < 3) {
        fprintf(stderr, "usage:%s ip port\n", argv[0]);
        return 0;
    }
    char const * ip = argv[1];
    int  port = atoi(argv[2]);
    fprintf(stderr, "listen to %s:%d", ip, port);
    server_t server;
    int ret = 0;
    ret = init_server(&server, ip, port);
    server.proc = &proc_echo; 
    start_server(&server);
    while (conet::get_epoll_pend_task_num() >0) {
        conet::epoll_once(-1);
    }
    return 0;
}

