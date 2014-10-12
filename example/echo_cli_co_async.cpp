/*
 * =====================================================================================
 *
 *       Filename:  echo_cli.cpp
 *
 *    Description:
 *
 *        Version:  1.0
 *        Created:  2014年05月11日 08时16分30秒
 *       Revision:  none
 *       Compiler:  gcc
 *
 *         Author:  YOUR NAME (),
 *   Organization:
 *
 * =====================================================================================
 */
#include <stdlib.h>
#include <stdlib.h>
#include <stdio.h>
#include <queue>

#include "thirdparty/gflags/gflags.h"

#include "base/incl/ip_list.h"
#include "base/incl/net_tool.h"

#include "base/incl/ref_str.h"
#include "core/incl/conet_all.h"

//using namespace conet;
DEFINE_string(server_addr, "127.0.0.1:12314", "server address");
DEFINE_int32(task_num, 10, "concurrent task num");
DEFINE_string(data_file, "1.txt", "send data file");

using namespace conet;

struct task_t
{
    std::string file;
    std::string ip;
    int port;
    conet::coroutine_t *co;
};

struct ctx_t
{
    int rfd;
    int wfd;
    size_t max_size;
    int total;
    std::queue<ref_str_t> read_queue;
    std::queue<ref_str_t> write_queue;
};

int read_co(void *arg)
{
    conet::enable_sys_hook();
    ctx_t * ctx = (ctx_t *)(arg);
    size_t size = ctx->max_size;
    char * buff = new char [size];

    int num = 0;
    int ret = 0;
    int cnt = 0;
    do
    {
        ret = read(ctx->rfd,  buff, size);
        if (ret <=0) {
            printf("read1 num:%d\n", num);
            break;
        }

        num += ret;
        ++cnt;

        if (cnt >= ctx->total) {
            printf("read2 num:%d\n", num);
            break;
        }
    } while(1);

    delete buff;
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
    int cnt = 0;
    do
    {
        if (ctx->write_queue.empty()) {
           break;
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
        ++cnt;
        if (cnt%100==0) {
            usleep(1);
        }
        ctx->read_queue.push(data);
    } while(1);
    printf("write num:%d\n", num);
    return 0;
}

int g_finish_task_num=0;

int proc_send(void *arg)
{
    conet::enable_sys_hook();
    ::task_t *task = (::task_t *)(arg);

    int ret = 0;
    int fd = 0;
    fd = conet::connect_to(task->ip.c_str(), task->port);
    conet::set_none_block(fd, false);
    char *line= NULL;
    size_t len = 0;
    char rbuff[1024];
    FILE *fp = fopen(task->file.c_str(), "r");
    if (!fp) {
        fprintf(stderr, "open file:%s failed!", task->file.c_str());
        ++g_finish_task_num;
        return -1;
    }
    
    ctx_t ctx;
    ctx.rfd = fd;
    ctx.wfd = fd;
    ctx.max_size = 1000;



    while(1) {
        char *buff = NULL;
        size_t size = 1000;
        ref_str_t data; 
        if (ctx.read_queue.empty()) {
            buff = new char[size];
            init_ref_str(&data, buff, size);
        } else {
            data = ctx.read_queue.front();
            ctx.read_queue.pop();
            buff = data.data;
        }
        ret = getline(&buff, &size, fp);
        if (ret <= 0) break;
        init_ref_str(&data, buff, ret);
        ctx.write_queue.push(data);
    }

    ctx.total = ctx.write_queue.size();

    coroutine_t *r_co  = alloc_coroutine(&read_co, &ctx);
    coroutine_t *w_co  = alloc_coroutine(&write_co, &ctx);

    conet::resume(w_co);
    conet::resume(r_co);

    conet::wait(w_co);

    conet::wait(r_co);
    close(fd);

    ++g_finish_task_num;
    return 0;
}

int main(int argc, char * argv[])
{
    google::ParseCommandLineFlags(&argc, &argv, false); 

    int num = FLAGS_task_num;

    std::vector<ip_port_t> ip_list;
    parse_ip_list(FLAGS_server_addr, &ip_list);
    if (ip_list.empty()) {
        fprintf(stderr, "server_addr:%s, format error!", FLAGS_server_addr.c_str());
        return 1;
    }

    ::task_t * tasks = new ::task_t[num];
    for (int i=0; i<num; ++i) {
        tasks[i].ip = ip_list[0].ip;
        tasks[i].port = ip_list[0].port;
        tasks[i].file = FLAGS_data_file;
        tasks[i].co = conet::alloc_coroutine(proc_send, tasks+i);
        resume(tasks[i].co);
    }

    while (g_finish_task_num < FLAGS_task_num) {
        conet::dispatch();
    }

    return 0;
}

