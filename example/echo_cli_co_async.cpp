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

std::vector<std::string *> g_data;

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
    std::vector<std::string *> *write_queue;
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
    for (size_t i = 0, len = ctx->write_queue->size(); i<len; ++i)
    {

        std::string * data = ctx->write_queue->at(i);
        buff   = (char *)data->c_str();

        size = data->size();

        ret = write(ctx->wfd,  buff, size);
        if (ret <=0) {
            break;
        }
        num+=ret;
        ++cnt;
        if (cnt%100==0) {
            usleep(1);
        }
    }
    printf("write num:%d\n", num);
    return 0;
}

int g_finish_task_num=0;

int proc_send(void *arg)
{
    conet::enable_sys_hook();
    ::task_t *task = (::task_t *)(arg);

    int fd = 0;
    fd = conet::connect_to(task->ip.c_str(), task->port);
    conet::set_none_block(fd, false);
    
    ctx_t ctx;
    ctx.rfd = fd;
    ctx.wfd = fd;
    ctx.max_size = 1000;


    ctx.total = g_data.size();
    ctx.write_queue = &g_data;

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


int prepare_data(char const *file)
{
    char *line= NULL;
    char rbuff[1024];
    FILE *fp = fopen(file, "r");
    if (!fp) {
        fprintf(stderr, "open file:%s failed!", file); 
        return -1;
    }
    int ret = 0;
    
    while(1) {
        size_t size = 1000;
        ret = getline(&line, &size, fp);
        if (ret <= 0) break;
        g_data.push_back(new std::string(rbuff, ret));
    }

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

    if (prepare_data(FLAGS_data_file.c_str())) {
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

