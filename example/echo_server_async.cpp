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
#include <errno.h>
#include <string.h>
#include <unistd.h>
#include <queue>

#include "svrkit/incl/server_base.h"
#include "thirdparty/gflags/gflags.h"

#include "base/plog.h"
#include "base/incl/ip_list.h"
#include "base/incl/ref_str.h"
#include "core/incl/conet_all.h"

using namespace conet;


struct ctx_t
{
    conn_info_t *conn;
    int rfd;
    int wfd;
    size_t max_size;
    int exit;
    std::queue<ref_str_t> read_queue;
    std::queue<ref_str_t> write_queue;
};

int read_co(void *arg)
{
    conet::enable_sys_hook();
    ctx_t * ctx = (ctx_t *)(arg);
    char * buff = NULL;
    size_t size = ctx->max_size;

    int ret = 0;
    int num=0;
    do
    {
        ref_str_t data; 
        if (ctx->read_queue.empty()) {
            buff = new char[size];
            init_ref_str(&data, buff, size);
        } else {
            data = ctx->read_queue.front();
            ctx->read_queue.pop();
            buff = data.data;
        }

        ret = read(ctx->rfd,  buff, size);
        if (ret <=0) {
            break;
        }

        init_ref_str(&data, buff, ret);

        num+=ret;
        ctx->write_queue.push(data);
    } while(1);
    ctx->exit = 1;
    PLOG_ERROR("read num=", num);
    return 0;
}

int write_co(void *arg)
{
    conet::enable_sys_hook();
    ctx_t * ctx = (ctx_t *)(arg);
    char * buff = NULL;
    size_t size = ctx->max_size;

    int num = 0;
    int ret = 0;
    do
    {
        if (ctx->write_queue.empty()) {
           if (ctx->exit >=1) break;
           usleep(1000); 
           continue;
        } 

        ref_str_t data = ctx->write_queue.front();
        buff   = data.data;
        size = data.len;

        ctx->write_queue.pop();

        ret = write(ctx->wfd,  buff, size);
        if (ret <=0) {
            break;
        }
        num+=ret;
        ctx->read_queue.push(data);
    } while(1);
    PLOG_ERROR("write num=", num);
    return 0;
}

inline
int proc_echo(conn_info_t *conn)
{
    conet::enable_sys_hook();
    server_t * server= conn->server;
    int size = server->conf.max_packet_size;

    ctx_t ctx;
    ctx.rfd = conn->fd;
    ctx.wfd = conn->fd;
    ctx.max_size = size;
    ctx.conn = conn;
    ctx.exit = 0;

    coroutine_t *r_co  = alloc_coroutine(&read_co, &ctx);
    coroutine_t *w_co  = alloc_coroutine(&write_co, &ctx);


    conet::resume(r_co);

    conet::resume(w_co);

    conet::wait(r_co);

    conet::wait(w_co);

    return 0;
}

DEFINE_string(server_addr, "0.0.0.0:12314", "server address");

int main(int argc, char * argv[])
{
    gflags::ParseCommandLineFlags(&argc, &argv, false); 

    std::vector<ip_port_t> ip_list;
    parse_ip_list(FLAGS_server_addr, &ip_list);
    if (ip_list.empty()) {
        fprintf(stderr, "server_addr:%s, format error!", FLAGS_server_addr.c_str());
        return 1;
    }

    server_t server;
    int ret = 0;
    ret = init_server(&server, ip_list[0].ip.c_str(), ip_list[0].port);
    server.proc = &proc_echo;
    start_server(&server);
    while (1) {
        conet::dispatch();
    }
    return 0;
}

