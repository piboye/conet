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
DEFINE_string(server_address, "0.0.0.0:12314", "default server address");
DEFINE_int32(server_stop_wait_seconds, 2, "server stop wait seconds");
DEFINE_int32(thread_num, 1, "server thread num");
DEFINE_string(cpu_set, "", "cpu affinity set");

uint64_t g_cnt = 0;

static int g_server_stop_flag = 0;

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

using namespace conet;
namespace conet {
    struct string_equal : public std::equal_to<>
    {
	using is_transparent = void;
    };

    struct string_hash
    {
        using is_transparent = void;
        using key_equal = std::equal_to<>;             // Pred to use
        using hash_type = std::hash<std::string_view>; // just a helper local type
        size_t operator()(std::string_view txt) const { return hash_type{}(txt); }
        size_t operator()(const std::string &txt) const { return hash_type{}(txt); }
        size_t operator()(const char *txt) const { return hash_type{}(txt); }
    };

    int proc_kv(void *arg, conn_info_t *conn);

    static std::string_view s_redis_ok = "+OK\r\n";
    static std::string_view s_reids_err = "-ERR unknown command 'helloworld'\r\n";
    static std::string_view s_get_result = "$";
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
        int cpu_id;
        uint64_t cnt;

        std::unordered_map<std::string, std::string, conet::string_hash, conet::string_equal> m_kv;

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
            return NULL;
        }

        int proc_get(redis_parser_t *req, int fd) const {
            auto key = req->args[0];
            auto it =  this->m_kv.find(key);
            //PLOG_INFO("get ", std::string(key));
            if (it == this->m_kv.end()) {
                return rsp("$0\r\n\r\n", fd);
            }


            auto const & value = it->second;

            //PLOG_INFO("get ", std::string(key), std::string(value));

            StringBuilder<4*1024> out;
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
                    PLOG_ERROR("unsuppored,", req);
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
        char *buff = GC_ALLOC_ARRAY(char, size);
        int ret = 0;
        int fd = conn->fd;

        ssize_t nparsed = 0;
        redis_parser_t req;
        do {
            req.init();
            do {
                ret = poll_recv(fd, buff, size-1, 10*1000);
            } while (ret == -1 && errno == EINTR);
            if (ret <= 0)
                break;

            //buff[ret] = '\0';
            
            nparsed += redis_parser_exec(&req, buff, ret);

            //PLOG_INFO("recv", std::string(buff, ret));

            ret = redis_parser_finish(&req);


            switch (ret)
            {
            case 1: // finished;
                {
                    task->proc_cmd(&req, fd);
                }
                /* code */
                break;
            default:
                break;
            }
            task->cnt++;
        } while (1);
        return 0;
    }
}

DEFINE_string(server_addr, "0.0.0.0:12314", "server address");

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

    std::vector<int> cpu_set;
    parse_affinity(FLAGS_cpu_set.c_str(), &cpu_set);

    int num = FLAGS_thread_num;
    Task *tasks = new Task[num];
    for (int i = 0; i < num; ++i)
    {
        tasks[i].ip_list = ip_list;
        if (!cpu_set.empty())
        {
            tasks[i].cpu_id = cpu_set[i % cpu_set.size()];
        }
        pthread_create(&tasks[i].tid, NULL, &Task::proc, tasks + i);
    }

    CO_RUN((tasks, num), {
        uint64_t prev_cnt = 0;
        while (!get_server_stop_flag())
        {
            uint64_t cur_cnt = 0;
            for (int i = 0; i < num; ++i)
            {
                cur_cnt += tasks[i].cnt;
            }
            int cnt = cur_cnt - prev_cnt;
            prev_cnt = cur_cnt;
            if (cnt > 0)
                PLOG_INFO("qps:", cnt);
            sleep(1);
        }
    });

    int exit_finished = 0;
    while (likely((exit_finished < 2)))
    {
        if (unlikely(get_server_stop_flag() && exit_finished == 0))
        {
            exit_finished = 1;
            CO_RUN((exit_finished, tasks, num), {
                PLOG_INFO("server ready exit!");
                for (int i = 0; i < num; ++i)
                {
                    Task::proc_server_exit(tasks + i);
                }
                exit_finished = 2;
            });
        }
        conet::dispatch();
    }

    for (int i = 0; i < num; ++i)
    {
        pthread_join(tasks[i].tid, NULL);
    }

    delete[] tasks;

    return 0;
}
