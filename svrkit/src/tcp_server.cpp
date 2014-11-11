/*
 * =====================================================================================
 *
 *       Filename:  tcp_server.cpp
 *
 *    Description:
 *
 *        Version:  1.0
 *        Created:  2014年05月10日 15时55分50秒
 *       Revision:  none
 *       Compiler:  gcc
 *
 *         Author:  piboye
 *   Organization:
 *
 * =====================================================================================
 */
#include <stdlib.h>
#include <stdint.h>
#include <string>
#include <netinet/tcp.h>
#include <fcntl.h>

#include "conet_all.h"
#include "tcp_server.h"
#include "gflags/gflags.h"
#include "glog/logging.h"

#include "base/incl/net_tool.h"
#include "base/incl/fn_ptr_cast.h"
#include "core/incl/fd_ctx.h"

DEFINE_int32(listen_backlog, 10000, "default listen backlog");
DEFINE_int32(max_conn_num, 100000, "default max conn num");
DEFINE_int32(max_packet_size, 1*4096, "default max packet size");
DEFINE_bool(enable_defer_accept, false, "enable TCP_DEFER_ACCEPT");
DEFINE_int32(accept_num, 100, "call accept num in one loop");

namespace conet
{

int conn_proc_co(conn_info_t *info)
{
    conet::enable_sys_hook();
    conet::enable_pthread_hook();
    conet::coroutine_t *co = CO_SELF();
    info = (conn_info_t *) conet::get_yield_value(co);
    tcp_server_t *server = (tcp_server_t *)info->server;

    tcp_server_t::conn_proc_cb_t conn_proc = server->conn_proc_cb;
    void * cb_arg = server->cb_arg;

    do
    {   //running;

        int ret = 0;

        ret = conn_proc(cb_arg, info);
        if (info->fd >=0) {
            close(info->fd);
            info->fd = -1;
        }
        co = info->co;
        --server->data.cur_conn_num;
        server->conn_info_pool.release(info);
        info = NULL;

        if (ret) break;
        if (server->to_stop) break;

        server->co_pool.release(co);
        info = (conn_info_t *) conet::yield(NULL);
    } while(!server->to_stop);

    if (info) {
        if (info->fd >=0) {
            close(info->fd);
            info->fd = -1;
        }
        --server->data.cur_conn_num;
        server->conn_info_pool.release(info);
        info = NULL;
    }

    return 0;
}



int proc_pool(tcp_server_t *server, conn_info_t *conn_info)
{
    conn_info->co = (coroutine_t *)server->co_pool.alloc();
    conet::resume(conn_info->co, conn_info);
    return 0;
}

static 
void * alloc_server_work_co(void *arg)
{
    conet::coroutine_t * co = alloc_coroutine((int (*)(void *))conn_proc_co, NULL);
    set_auto_delete(co);
    return co;
}

static 
void free_server_work_co(void *arg, void * val)
{
    conet::coroutine_t * co = (conet::coroutine_t *) (val);
    resume(co, NULL);
}

int tcp_server_t::init(const char *ip, int port, int listen_fd)
{
    tcp_server_t *server = this;
    server->ip = ip;
    server->port = port;
    server->state = SERVER_START;
    server->main_co = NULL;
    server->extend = NULL;
    server->conf.listen_backlog = FLAGS_listen_backlog;
    server->conf.max_conn_num = FLAGS_max_conn_num;
    server->conf.max_packet_size = FLAGS_max_packet_size;
    server->data.cur_conn_num = 0;
    server->to_stop = 0;
    server->listen_fd = listen_fd;
    server->co_pool.set_alloc_obj_func(alloc_server_work_co, server);
    server->co_pool.set_free_obj_func(free_server_work_co, server);
    server->accept_fd_queue = NULL;

    return 0;
}



int tcp_server_t::start()
{
    this->main_co = alloc_coroutine(conet::fn_ptr_cast<co_main_func_t>(&tcp_server_t::main_proc), this);
    conet::resume(this->main_co);
    return 0;
}


int tcp_server_t::main_proc()
{
    conet::enable_sys_hook();
    conet::enable_pthread_hook();

    this->state = tcp_server_t::SERVER_RUNNING;

    if (this->accept_fd_queue == NULL) {  
        return this->main_proc2();
    } else {
        return this->main_proc_with_fd_queue();
    }
}

int tcp_server_t::main_proc_with_fd_queue()
{
    std::vector<int> new_fds;
    conn_info_t * conn_info = this->conn_info_pool.alloc();

    FdQueue *fd_queue = this->accept_fd_queue;

    while (0==this->to_stop) 
    {
        while (this->data.cur_conn_num >= this->conf.max_conn_num) 
        {
            usleep(10000); // block 10ms
            if (this->to_stop) 
            {
                break;
            }
        }

        if (fd_queue->empty()) {
            usleep(1000); // 1ms;
        }

        int fd = fd_queue->pop_fd();
        if (fd >=0) {
            set_nodelay(fd);
            set_none_block(fd);
            ++this->data.cur_conn_num;

            conn_info->server = this;

            conn_info->fd = fd;

            proc_pool(this, conn_info);

            conn_info = this->conn_info_pool.alloc();
        }
    }

    if (conn_info) {
        delete conn_info;
    }

    this->state = SERVER_STOPED;
    return 0;
}

int tcp_server_t::main_proc2()
{
    
    // 不使用 fd pool 

    int listen_fd = this->listen_fd; 
    if (listen_fd <0) 
    {
        listen_fd = create_tcp_socket(this->port, this->ip.c_str(), true);
        if (listen_fd <0) 
        {
            this->state = SERVER_STOPED;
            LOG(ERROR)<<"create listen socket failed, "
                "["<<this->ip<<":"<<this->port<<"]"
                "[errno:"<<errno<<"]"
                "[errmsg:"<<strerror(errno)<<"]";
            return -1;
        }

        this->listen_fd = listen_fd;
    } 

    set_none_block(listen_fd, true);

    listen(listen_fd, this->conf.listen_backlog); 

    if (FLAGS_enable_defer_accept) { 
        int waits = 1; // 1 seconds;
        setsockopt(listen_fd, IPPROTO_IP, TCP_DEFER_ACCEPT, &waits, sizeof(waits));
    }

    int ret = 0;

    std::vector<int> new_fds;
    conn_info_t * conn_info = this->conn_info_pool.alloc();
    int accept_num = FLAGS_accept_num;
    while (0==this->to_stop) 
    {
        while (this->data.cur_conn_num >= this->conf.max_conn_num) 
        {
            usleep(10000); // block 10ms
            if (this->to_stop) 
            {
                break;
            }
        }
        struct pollfd pf = { 0 };
        pf.fd = listen_fd;
        pf.events = (POLLIN|POLLERR|POLLHUP);
        ret = co_poll(&pf, 1, 1000);
        if (ret == 0) continue;
        if (ret <0) {
            if (errno == EINTR) {
                continue;
            }
            break;
        }

        socklen_t len = sizeof(conn_info->addr);

        new_fds.clear();
        for(int i=0; i<accept_num; ++i)
        {
            int fd = accept4(listen_fd, (struct sockaddr *)&conn_info->addr, &len, O_NONBLOCK);
            if (fd <0) break;
            new_fds.push_back(fd);
        } 

        for (size_t i=0; i<new_fds.size(); ++i)
        {
            int fd = new_fds[i];
            set_nodelay(fd);
            ++this->data.cur_conn_num;
            //memset(conn_info, 0, sizeof(conn_info_t));

            conn_info->server = this;
            //memcpy(&conn_info->addr, &addr,len);

            conn_info->fd = fd;

            proc_pool(this, conn_info);

            conn_info = this->conn_info_pool.alloc();
        }
    }

    if (conn_info) {
        delete conn_info;
    }

    close(listen_fd);
    this->state = SERVER_STOPED;
    return 0;
}

int tcp_server_t::stop(int wait_ms)
{
    tcp_server_t *server = this;

    server->to_stop = 1;
    if (server->state == SERVER_STOPED) {
        return 0;
    }

    conet::wait(server->main_co, 20);
    if (wait_ms >0) {
        for (int i=0; i< wait_ms; i+=1000) {
            if (server->data.cur_conn_num <= 0) break;
            LOG(INFO)<<"wait server["<<server->ip<<":"<<server->port << "] conn exit";
            sleep(1);
        }
    } else {
        while(1) {
            if (server->data.cur_conn_num <= 0) break;
            LOG(INFO)<<"wait server["<<server->ip<<":"<<server->port << "] conn exit";
            sleep(1);
        }
    }

    server->state = SERVER_STOPED;

    if (server->data.cur_conn_num > 0) {
        LOG(ERROR)<<"server["<<server->ip<<":"<<server->port
            <<"] exit, but leak conn num:"<<server->data.cur_conn_num; 
        return -1;
    }
    return 0;
}

}
