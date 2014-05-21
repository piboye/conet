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
#include "core/incl/conet_all.h"
#include "core/incl/net_tool.h"
#include "server_base.h"


namespace conet
{

void init_co_pool(co_pool_t *pool, int max_num)
{
    INIT_LIST_HEAD(&pool->free_list);
    INIT_LIST_HEAD(&pool->used_list);
    pool->total_num = 0;
    pool->max_num = max_num;
}

int client_proc(conn_info_t *info) 
{
    co_pool_item_t pool_item;   
    INIT_LIST_HEAD(&pool_item.link);
    pool_item.co = info->co; 
    server_t *server = info->server;
    co_pool_t *pool = &server->co_pool; 
    ++pool->total_num;
    
    while(server->state == 0) 
    { //running;

       list_add(&pool_item.link, &pool->used_list);

       int ret = 0;

       ret = server->proc(info); 
       close(info->fd);
       delete info;
       info = NULL;

       if (ret) break;
       list_del_init(&pool_item.link);

       if (pool->total_num > pool->max_num) break;
       list_add(&pool_item.link, &pool->free_list);
       info = (conn_info_t *) conet::yield(0);
    }

    list_del_init(&pool_item.link);
    --pool->total_num;
    return 0;
}



int proc_pool(server_t *server, conn_info_t *conn_info)
{
    co_pool_t *pool = &server->co_pool; 
    
    if (list_empty(&pool->free_list)) 
    {
       if (pool->total_num + 1 < pool->max_num) {
           conn_info->co = alloc_coroutine((int (*)(void *))client_proc, conn_info);
           set_auto_delete(conn_info->co);
           resume(conn_info->co, conn_info);
           return 0;
       } else {
           while  (list_empty(&pool->free_list)) {
               usleep(1000);
           }
       }
    } 

    list_head * it = pool->free_list.next;
    list_del_init(it);
    co_pool_item_t *item = container_of(it, co_pool_item_t, link);
    conet::resume(item->co, conn_info);
    return 0;
}

int init_server(server_t *server, const char *ip, int port, int max_packet_size)
{
    server->ip = ip;
    server->port = port;
    server->state = 0;
    server->co = NULL;
    server->max_packet_size = max_packet_size;
    init_co_pool(&server->co_pool, 10000);
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

    int listen_fd = create_tcp_socket(server->port, server->ip.c_str());
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

        struct sockaddr_in addr; 
        memset( &addr,0,sizeof(addr) );
        socklen_t len = sizeof(addr);

        int fd = accept(listen_fd, (struct sockaddr *)&addr, &len);

        if (fd <0) continue;

        conn_info_t *conn_info = new conn_info_t();
        conn_info->server = server;
        memcpy(&conn_info->addr, &addr,len);
        conn_info->fd = fd;

        proc_pool(server, conn_info);
    }
    return 0;
}

}
