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
#include "thirdparty/gflags/gflags.h"
#include "svrkit/ip_list.h"

//using namespace conet;
DEFINE_string(server_addr, "127.0.0.1:12314", "server address");
DEFINE_int32(task_num, 10, "concurrent task num");
DEFINE_string(data_file, "1.txt", "send data file");

struct task_t
{
    std::string file;
    std::string ip;
    int port;
    conet::coroutine_t *co;
};

int proc_send(void *arg)
{
    conet::enable_sys_hook();
    task_t *task = (task_t *)(arg);

    int ret = 0;
    int fd = 0;
    fd = connect_to(task->ip.c_str(), task->port);
    set_none_block(fd, false);
    char *line= NULL;
    size_t len = 0;
    char rbuff[1024];
    FILE *fp = fopen(task->file.c_str(), "r");
    if (!fp) {
        fprintf(stderr, "open file:%s failed!", task->file.c_str());
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

    task_t * tasks = new task_t[num];
    for (int i=0; i<num; ++i) {
        tasks[i].ip = ip_list[0].ip;
        tasks[i].port = ip_list[0].port;
        tasks[i].file = FLAGS_data_file;
        tasks[i].co = conet::alloc_coroutine(proc_send, tasks+i);
        resume(tasks[i].co);
    }

    while (conet::get_epoll_pend_task_num() >0) {
        conet::dispatch();
    }

    return 0;
}

