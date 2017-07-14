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
 *         Author:  piboye
 *   Organization:
 *
 * =====================================================================================
 */
#include <stdlib.h>
#include <stdlib.h>
#include <stdio.h>
#include "core/conet_all.h"
#include "thirdparty/gflags/gflags.h"
#include "base/plog.h"

#include "base/ip_list.h"
#include "base/net_tool.h"

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
    std::vector<std::string *> *data;
};

int g_finish_task_num=0;

std::vector<std::string *> g_data;
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

int proc_send(void *arg)
{
    conet::enable_sys_hook();
    ::task_t *task = (::task_t *)(arg);

    int ret = 0;
    int fd = 0;
    char rbuff[1024];
    fd = conet::connect_to(task->ip.c_str(), task->port);
    conet::set_none_block(fd, false);

    for (int i=0, len = task->data->size(); i<len; ++i) {
        std::string * send_data = task->data->at(i); 
        ret = write(fd, send_data->c_str(), send_data->size());
        if (ret <= 0) break;
        ret = read(fd, rbuff, 1024);
        if (ret <=0) break;
    }
    ++g_finish_task_num;
    return 0;
}

::task_t *tasks = NULL;

int main(int argc, char * argv[])
{
    gflags::ParseCommandLineFlags(&argc, &argv, false); 

    int num = FLAGS_task_num;

    std::vector<ip_port_t> ip_list;
    parse_ip_list(FLAGS_server_addr, &ip_list);
    if (ip_list.empty()) {
        fprintf(stderr, "server_addr:%s, format error!", FLAGS_server_addr.c_str());
        return 1;
    }

    if (prepare_data(FLAGS_data_file.c_str())) {
        PLOG_ERROR("read data failed!");
        return 1;
    }

    tasks = new ::task_t[num];
    for (int i=0; i<num; ++i) {
        tasks[i].ip = ip_list[0].ip;
        tasks[i].port = ip_list[0].port;
        tasks[i].file = FLAGS_data_file;
        tasks[i].co = conet::alloc_coroutine(proc_send, tasks+i);
        tasks[i].data = &g_data;
        resume(tasks[i].co);
    }

    while (g_finish_task_num < FLAGS_task_num) {
        conet::dispatch();
    }

    return 0;
}

