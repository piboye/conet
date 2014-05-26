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
#include "net_tool.h"
#include "conet_all.h"

//using namespace conet;

struct task_t
{
    char const *file;
    char const * ip;
    int port;
    conet::coroutine_t *co;
};

int proc_send(void *arg)
{
    conet::enable_sys_hook();
    task_t *task = (task_t *)(arg);

    int ret = 0;
    int fd = 0;
    fd = connect_to(task->ip, task->port);
    set_none_block(fd, false);
    char *line= NULL;
    size_t len = 0;
    char rbuff[1024];
    FILE *fp = fopen(task->file, "r");
    if (!fp) {
        fprintf(stderr, "open file:%s failed!", task->file);
        return -1;
    }
    while( (ret = getline(&line, &len, fp)) >= 0) {
        if (ret == 0) continue;
        ret = write(fd, line, ret);
        if (ret <= 0) break;
        ret = read(fd, rbuff, 1024);
        if (ret <=0) break;
        //write(1, rbuff, ret);
    }
    return 0;
}

int main(int argc, char const* argv[])
{
    if (argc < 5) {
        fprintf(stderr, "usage:%s ip port num data_file\n", argv[0]);
        return 0;
    }
    char const * ip = argv[1];
    int  port = atoi(argv[2]);
    int num = atoi(argv[3]);
    char const * data_file = argv[4];
    task_t * tasks = new task_t[num];
    for (int i=0; i<num; ++i) {
        tasks[i].ip = ip;
        tasks[i].port = port;
        tasks[i].file = data_file;
        tasks[i].co = conet::alloc_coroutine(proc_send, tasks+i);
        resume(tasks[i].co);
    }

    while (conet::get_epoll_pend_task_num() >0) {
        conet::dispatch_one();
    }

    return 0;
}

