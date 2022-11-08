/*
 * =====================================================================================
 *
 *       Filename:  pkv.cpp
 *
 *    Description:
 *
 *        Version:  1.0
 *        Created:  2022年09月11日 11时50分16秒
 *       Revision:  none
 *       Compiler:  gcc
 *
 *         Author:  piboye@qq.com
 *   Organization:
 *
 * =====================================================================================
 */
#include <stdlib.h>
#include <stdio.h>
#include <errno.h>
#include <string.h>
#include <signal.h>
#include <unistd.h>
#include "svrkit/tcp_server.h"
#include "thirdparty/gflags/gflags.h"
#include "base/plog.h"
#include "base/cpu_affinity.h"
#include "base/module.h"
#include "base/ip_list.h"
#include "base/net_tool.h"
#include "core/coroutine.h"
#include "base/redis_parse.h"
#include <unordered_map>
#include <base/string_builder.h>

using namespace conet;
DEFINE_string(server_addr, "0.0.0.0:12314", "default server address");
DEFINE_int32(server_stop_wait_seconds, 2, "server stop wait seconds");
DEFINE_string(cpu_set, "", "cpu affinity set");

uint64_t g_cnt = 0;

static int g_server_stop_flag = 0;


using namespace conet;
namespace conet {
    std::string data = "hello";

    int proc_kv(void *arg, conn_info_t *conn);

    static std::string_view s_redis_ok = "+OK\r\n";
    static std::string_view s_reids_err = "-ERR unknown command\r\n";
    static std::string_view s_get_result = "$5\r\nhello\r\n";
    static std::string_view s_crln = "\r\n";

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
        int cpu_id = -1;
        uint64_t cnt;

        std::unordered_map<std::string, std::string> m_kv;//, conet::string_hash, conet::string_equal> m_kv;

        Task()
        {
            exit_finsished = 0;
            http_listen_fd = -1;
            rpc_listen_fd = -1;
            cpu_id = -1;
            cnt = 0;
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
            pthread_setcanceltype(PTHREAD_CANCEL_DISABLE, NULL);
            conet::init_conet_env();
            int ret = 0;
            Task *self = (Task *)arg;
            if (self->cpu_id >= 0)
            {
                set_cur_thread_cpu_affinity(self->cpu_id);
                self->server.cpu_id = self->cpu_id;
            }

            ret = self->server.init(self->ip_list[0].ip.c_str(), self->ip_list[0].port);
            if (ret)
            {
                PLOG_ERROR("init server faile!", (ret));
                return NULL;
            }
            self->server.set_conn_cb(proc_kv, NULL);
            self->server.start();
            while (!self->exit_finsished)
            {
                conet::dispatch();
            }

            self->server.stop();
            /*
            while (!self->server.has_stoped()) {
                conet::dispatch();
            }
            */
            return NULL;
        }

        int proc_get(redis_parser_t *req, int fd) const {
            auto const & key = req->args[0];
            auto it =  this->m_kv.find(std::string(key));
            //PLOG_INFO("get ", std::string(key));
            if (it == this->m_kv.end()) {
                return rsp("$0\r\n\r\n", fd);
            }

            auto const & value = it->second;
            //auto const & value = data;

            //PLOG_INFO("get ", std::string(key), std::string(value));

            StringBuilder<1024> out;
            out.append_ch('$');
            out.append(value.size());
            out.append(s_crln);
            out.append(value.data(), value.size());
            out.append(s_crln);
            return rsp(std::string_view(out.data(), out.size()), fd); 
           /*
            char buffer[1024];
            const auto result = std::format_to_n(buffer, sizeof(buffer), "${{1}}\r\n{{}}\r\n", value.size(), value);
            return rsp(buffer,  result.out - buffer, fd);
            */
        }

        static int rsp(std::string_view data, int fd) {
            return conet::send_data(fd, data.data(), data.size());
            //return write(fd, data.data(), data.size());
        }


        int proc_set(redis_parser_t *req, int fd) {
            auto key = req->args[0];
            auto value = req->args[1];

            //PLOG_INFO("set ", std::string(key), std::string(value));

            m_kv.insert(std::make_pair(key, value));

            return rsp(s_redis_ok, fd);
        }

        int proc_cmd(redis_parser_t * req, int fd) {
            auto cmd = req->cmd;
            switch(cmd) {
                case redis_parser_t::SET:
                    return proc_set(req, fd);
                  
                break;
                case redis_parser_t::GET:
                    return proc_get(req, fd);
                break;
                default:
                    //PLOG_ERROR("unsuppored,", req);
                    return rsp(s_reids_err, fd);
            }
            return 0;
        }
    };


    int proc_kv(void *arg, conn_info_t *conn)
    {
        conet::enable_sys_hook();
        tcp_server_t *svr = (tcp_server_t *)conn->server;
        Task *task = NULL;
        task = container_of(svr, Task, server);
        int size = svr->conf.max_packet_size;
        char *buff = (char*)aligned_alloc(64, size);
        int ret = 0;
        int fd = conn->fd;

        ssize_t nparsed = 0;
        ssize_t end =  0;
        ssize_t recved = 0;
        redis_parser_t req;

        //req.reinit();
        do {
            do {
                ret = poll_recv(fd, buff+nparsed, size-nparsed, 10000);
            } while (ret == -1 && errno == EINTR);
            if (ret <= 0) break;
            recved = ret;

            end=recved + nparsed;

            
            nparsed += redis_parser_exec(&req, buff, end, nparsed);

            ret = redis_parser_finish(&req);

            //PLOG_INFO("recv", std::string(buff, end), ret);

            switch (ret)
            {
            case 1: // finished;
                {
                    task->proc_cmd(&req, fd);
                    req.reinit();
                    nparsed = 0;
                    task->cnt++;
                }
                /* code */
                break;
            case 0: 
                //PLOG_INFO("continue");
                continue;
                break;
            default:
                goto p0;
                break;
            }
        } while (1);
p0:
        close(fd);
        free(buff);
        return 0;
    }
}

Task *ptask = NULL;

int set_server_stop()
{
    g_server_stop_flag = 1;
    return 0;
}

int get_server_stop_flag()
{
    return g_server_stop_flag;
}

static void sig_exit(int sig)
{
    set_server_stop();
}

int main(int argc, char *argv[])
{
    // conet::init_conet_global_env();

    InitAllModule(argc, argv);

    std::vector<ip_port_t> ip_list;
    parse_ip_list(FLAGS_server_addr, &ip_list);
    if (ip_list.empty())
    {
        fprintf(stderr, "server_addr:%s, format error!", FLAGS_server_addr.c_str());
        return 1;
    }

    signal(SIGINT, sig_exit);
    signal(SIGPIPE, SIG_IGN);

    Task task;
    ptask = &task;
    task.ip_list = ip_list;

    std::vector<int> cpu_set;
    parse_affinity(FLAGS_cpu_set.c_str(), &cpu_set);
    if (cpu_set.size() >0) {
        task.cpu_id = cpu_set[0];
    }

    CO_RUN((ptask), {
        uint64_t prev_cnt = 0;
        while (!get_server_stop_flag())
        {
            uint64_t cur_cnt = 0;
            cur_cnt = ptask->cnt;
            int cnt = cur_cnt - prev_cnt;
            prev_cnt = cur_cnt;
            if (cnt > 0)
                PLOG_INFO("qps:", cnt);
            sleep(1);
        }
    });
    
    pthread_create(&task.tid, NULL, &Task::proc, &task);

    int exit_finished = 0;
    while (likely((exit_finished < 2)))
    {
        if (unlikely(get_server_stop_flag() && exit_finished == 0))
        {
            exit_finished = 1;
            CO_RUN((exit_finished, ptask), {
                PLOG_INFO("server ready exit!");
                Task::proc_server_exit(ptask);
                exit_finished = 2;
            });
        }
        conet::dispatch();
    }


    pthread_join(task.tid, NULL);

    return 0;
}
