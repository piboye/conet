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
#include "conet_all.h"
#include "server_base.h"
#include "gflags/gflags.h"
#include "glog/logging.h"

#include "base/incl/net_tool.h"
#include "core/incl/fd_ctx.h"

DEFINE_int32(listen_backlog, 1000, "default listen backlog");
DEFINE_int32(max_conn_num, 100000, "default max conn num");
DEFINE_int32(max_packet_size, 102400, "default max packet size");

namespace conet
{


int client_proc(conn_info_t *info)
{
    conet::enable_sys_hook();
    conet::enable_pthread_hook();
    conet::coroutine_t *co = CO_SELF();
    info = (conn_info_t *) conet::get_yield_value(co);
    server_t *server = info->server;
    do
    {   //running;

        int ret = 0;

        ret = server->proc(info);
        close(info->fd);
        co = info->co;
        --server->data.cur_conn_num;
        server->conn_info_pool.release(info);
        info = NULL;

        if (ret) break;
        if (server->to_stop) break;

        server->co_pool.release(co);
        info = (conn_info_t *) conet::yield(NULL);
    } while(!server->to_stop);

    if (info) {
        close(info->fd);
        --server->data.cur_conn_num;
        server->conn_info_pool.release(info);
        info = NULL;
    }

    return 0;
}



int proc_pool(server_t *server, conn_info_t *conn_info)
{
    conn_info->co = (coroutine_t *)server->co_pool.alloc();
    conet::resume(conn_info->co, conn_info);
    return 0;
}

static 
void * alloc_server_work_co(void *arg)
{
    conet::coroutine_t * co = alloc_coroutine((int (*)(void *))client_proc, NULL);
    set_auto_delete(co);
    return co;
}

int init_server(server_t *server, const char *ip, int port)
{
    server->ip = ip;
    server->port = port;
    server->state = server_t::SERVER_START;
    server->co = NULL;
    server->extend = NULL;
    server->conf.listen_backlog = FLAGS_listen_backlog;
    server->conf.max_conn_num = FLAGS_max_conn_num;
    server->conf.max_packet_size = FLAGS_max_packet_size;
    server->data.cur_conn_num = 0;
    server->to_stop = 0;
    server->listen_fd = -1;
    server->co_pool.set_alloc_obj_func(alloc_server_work_co, server);
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
    conet::enable_pthread_hook();
    server->state = server_t::SERVER_RUNNING;

    int listen_fd = server->listen_fd; 
    if (listen_fd <0) 
    {
        listen_fd = create_tcp_socket(server->port, server->ip.c_str(), true);
        if (listen_fd <0) 
        {
            server->state = server_t::SERVER_STOPED;
            return -1;
        }
        
        server->listen_fd = listen_fd;

    } 

    set_none_block(listen_fd, true);

    listen(listen_fd, server->conf.listen_backlog); 

    int waits = 5; // 5 seconds;
    setsockopt(listen_fd, IPPROTO_IP, TCP_DEFER_ACCEPT, &waits, sizeof(waits));

    int ret = 0;
    while (0==server->to_stop) {
        while (server->data.cur_conn_num >= server->conf.max_conn_num) {
            usleep(10000); // block 10ms
            if (server->to_stop) {
                break;
            }
        }
        struct pollfd pf = { 0 };
        pf.fd = listen_fd;
        pf.events = (POLLIN|POLLERR|POLLHUP);
        ret = poll(&pf, 1, 1000);
        if (ret == 0) continue;
        if (ret <0) {
            if (errno == EINTR) {
                continue;
            }
            break;
        }

        struct sockaddr_in addr;
        memset( &addr, 0, sizeof(addr) );
        socklen_t len = sizeof(addr);

        int fd = accept(listen_fd, (struct sockaddr *)&addr, &len);

        if (fd <0) continue;

        ++server->data.cur_conn_num;

        conn_info_t *conn_info = server->conn_info_pool.alloc();
        //memset(conn_info, 0, sizeof(conn_info_t));
        conn_info->server = server;
        memcpy(&conn_info->addr, &addr,len);
        conn_info->fd = fd;

        proc_pool(server, conn_info);
    }
    close(listen_fd);
    server->state = server_t::SERVER_STOPED;
    return 0;
}

int stop_server(server_t *server, int wait_ms)
{
    server->to_stop = 1;
    if (server->state == server_t::SERVER_STOPED) {
        return 0;
    }
    conet::wait(server->co, 20);
    if (wait_ms >0) {
        for (int i=0; i< wait_ms; i+=1000) {
            if (server->data.cur_conn_num <= 0) break;
            LOG(INFO)<<"wait server["<<server->ip<<":"<<server->port << "] conn exit";
            sleep(1);
        }
    } else {
        while(1) {
            if (server->data.cur_conn_num <= 0) break;
            LOG(INFO)<<"wait server["<<server->ip<<":"<<server->port << "] conn exit";
            sleep(1);
        }
    }

    server->state = server_t::SERVER_STOPED;

    if (server->data.cur_conn_num > 0) {
        LOG(ERROR)<<"server["<<server->ip<<":"<<server->port
            <<"] exit, but leak conn num:"<<server->data.cur_conn_num; 
        return -1;
    }
    return 0;
}

}
