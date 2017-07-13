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
#include "svrkit/tcp_server.h"
#include "thirdparty/glog/logging.h"
#include "thirdparty/gflags/gflags.h"
#include "base/cpu_affinity.h"
#include "base/delay_init.h"
#include "base/ip_list.h"
#include "base/net_tool.h"

using namespace conet;
DEFINE_string(server_address, "0.0.0.0:12314", "default server address");
DEFINE_int32(server_stop_wait_seconds, 2, "server stop wait seconds");
DEFINE_int32(thread_num, 1, "server thread num");
DEFINE_string(cpu_set, "", "cpu affinity set");


inline
int proc_echo(void *arg, conn_info_t *conn)
{
    conet::enable_sys_hook();
    tcp_server_t * server= (tcp_server_t *)conn->server;
    int size = server->conf.max_packet_size;
    char * buff = GC_ALLOC_ARRAY(char, size);
    int ret = 0;
    do
    {
        ret = read(conn->fd,  buff, size);
        if (ret <=0) {
            break;
        }

        ret = write(conn->fd, buff, ret);
        if (ret <=0) break;
    } while(1);
    //free(buff);
    return 0;
}
namespace 
{
struct Task
{
    public:    
        pthread_t tid;
        int exit_finsished;
        std::vector<ip_port_t> ip_list, http_ip_list;
        std::string http_address;

        tcp_server_t server;
        int http_listen_fd;
        int rpc_listen_fd;
        int cpu_id;

        Task()
        {
            exit_finsished = 0;
            http_listen_fd = -1;
            rpc_listen_fd = -1;
            cpu_id = -1;
        }

        static int proc_server_exit(void *arg)
        {
            Task *self = (Task *)(arg);
            self->server.stop();
            self->exit_finsished = 1;
            return 0;
        }

        static void *proc(void *arg)
        {
            int ret = 0;

            Task *self = (Task *)arg;
            if (self->cpu_id >=0) {
                set_cur_thread_cpu_affinity(self->cpu_id);
            }

            ret = self->server.init(self->ip_list[0].ip.c_str(), self->ip_list[0].port);
            if (ret) {
                LOG(ERROR)<<"init server faile! [ret:"<<ret<<"]";
                return NULL;
            }
            self->server.set_conn_cb(proc_echo, NULL);
            self->server.start();
            while (1) 
            {
                conet::dispatch();
            }
            return NULL;
        }

};
}

DEFINE_string(server_addr, "0.0.0.0:12314", "server address");

int main(int argc, char * argv[])
{
    gflags::ParseCommandLineFlags(&argc, &argv, false); 
    google::InitGoogleLogging(argv[0]);

    std::vector<ip_port_t> ip_list;
    parse_ip_list(FLAGS_server_addr, &ip_list);
    if (ip_list.empty()) {
        fprintf(stderr, "server_addr:%s, format error!", FLAGS_server_addr.c_str());
        return 1;
    }

    std::vector<int> cpu_set;
    parse_affinity(FLAGS_cpu_set.c_str(), &cpu_set);

    {
        // delay init
        delay_init::call_all_level();
        LOG(INFO)<<"delay init total:"<<delay_init::total_cnt
                <<" success:"<<delay_init::success_cnt
                <<", failed:"<<delay_init::failed_cnt;

        if(delay_init::failed_cnt>0)
        {
            LOG(ERROR)<<"delay init failed, failed num:"<<delay_init::failed_cnt;
            return 1;
        }
    }

    if (FLAGS_thread_num <= 1) {
        Task task;
        task.ip_list = ip_list;
        if (!cpu_set.empty()) {
            task.cpu_id = cpu_set[0];
        }
        task.proc(&task);
    } else {

#if HAVE_SO_REUSEPORT
        int num = FLAGS_thread_num;
        Task *tasks = new Task[num];
        for (int i=0; i< num; ++i)
        {
            tasks[i].ip_list = ip_list;
            if (!cpu_set.empty()) {
                tasks[i].cpu_id = cpu_set[i%cpu_set.size()];
            }
            pthread_create(&tasks[i].tid, NULL, &Task::proc, tasks+i);
        }
#else

        int rpc_listen_fd = conet::create_tcp_socket(ip_list[0].port, ip_list[0].ip.c_str(), true);

        int num = FLAGS_thread_num;
        Task *tasks = new Task[num];
        for (int i=0; i< num; ++i)
        {
            if (!cpu_set.empty()) {
                tasks[i].cpu_id = cpu_set[i%cpu_set.size()];
            }
            tasks[i].ip_list = ip_list;

            tasks[i].rpc_listen_fd = dup(rpc_listen_fd);

            pthread_create(&tasks[i].tid, NULL, &Task::proc, tasks+i);
        }
#endif

        for (int i=0; i< num; ++i)
        {
            pthread_join(tasks[i].tid, NULL);
        }

        delete[] tasks;
    }

    return 0;
}
