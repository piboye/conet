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
#include "base/plog.h"

#include "base/net_tool.h"
#include "base/ptr_cast.h"
#include "core/fd_ctx.h"

DEFINE_int32(listen_backlog, 10000, "default listen backlog");
DEFINE_int32(max_conn_num, 100000, "default max conn num");
DEFINE_int32(max_packet_size, 100*1024, "default max packet size");
DEFINE_bool(enable_defer_accept, false, "enable TCP_DEFER_ACCEPT");
DEFINE_int32(accept_num, 100, "call accept num in one loop");

namespace conet
{

tcp_server_t::tcp_server_t()
{
    this->listen_fd = -1;
    this->main_co = NULL;
    this->extend = NULL;
    this->conn_proc_cb = NULL;
    this->cb_arg = NULL;
    this->exit_wait_ms = 10*1000;
    this->port = 0;
}

tcp_server_t::~tcp_server_t()
{
    if (this->main_co)
    {
        free_coroutine(this->main_co);
        this->main_co = NULL;
    }
}

static
int conn_proc_co(void *)
{
    conet::enable_sys_hook();
    conet::enable_pthread_hook();

    conet::coroutine_t *co = CO_SELF();
    conn_info_t * info = (conn_info_t *)yield();
    if (info == NULL)
    {
        return 0;
    }

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
    } while(info && !server->to_stop);

    if (info) {
        if (info->fd >=0) {
            close(info->fd);
            info->fd = -1;
        }
        server->conn_info_pool.release(info);
        info = NULL;
        --server->data.cur_conn_num;
    }

    return 0;
}



static
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
    resume(co, NULL);
    return co;
}

static 
void free_server_work_co(void *arg, void * val)
{
    conet::coroutine_t * co = (conet::coroutine_t *) (val);
    resume(co, NULL);
}

int tcp_server_t::init(char const *ip, int port, int listen_fd)
{
    tcp_server_t *server = this;
    server->ip = ip;
    server->port = port;
    server->main_co = NULL;
    server->extend = NULL;
    server->conf.listen_backlog = FLAGS_listen_backlog;
    server->conf.max_conn_num = FLAGS_max_conn_num;
    server->conf.max_packet_size = FLAGS_max_packet_size;
    server->conf.duplex = 0; // 默认禁用多路复用
    server->data.cur_conn_num = 0;
    server->listen_fd = listen_fd;
    server->co_pool.set_alloc_obj_func(alloc_server_work_co, server);
    server->co_pool.set_free_obj_func(free_server_work_co, server);
    server->accept_fd_queue = NULL;

    return 0;
}



int tcp_server_t::start()
{
    if (this->main_co) {
        PLOG_ERROR("has been start");
        return 0;
    }

    this->main_co = alloc_coroutine(
            conet::ptr_cast<co_main_func_t>(&tcp_server_t::main_proc_help), 
            this);
    //conet::set_auto_delete(this->main_co);
    conet::resume(this->main_co);
    return 0;
}


int tcp_server_t::main_proc_help()
{
    conet::enable_sys_hook();
    conet::enable_pthread_hook();

    this->state = tcp_server_t::SERVER_RUNNING;

    if (this->accept_fd_queue == NULL) {  
        return this->main_proc();
    } else {
        return this->main_proc_with_fd_queue();
    }
}

int tcp_server_t::main_proc_with_fd_queue()
{
    std::vector<int> new_fds;
    conn_info_t * conn_info = this->conn_info_pool.alloc();

    UnixSocketSendFd *fd_queue = this->accept_fd_queue;

    int unix_fd = fd_queue->get_recv_handle();
    set_none_block(unix_fd, true);
    int ret =0;

    int accept_num = FLAGS_accept_num;
    std::vector<int> fds;
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
        pf.fd = unix_fd;
        pf.events = (POLLIN|POLLERR|POLLHUP);
        ret = co_poll(&pf, 1, 1000);
        if (ret == 0) continue;
        if (ret <0) {
            if (errno == EINTR) {
                continue;
            }
            break;
        }

        for (int i=0; i<accept_num; ++i)
        {
            fds.clear();
            ret = fd_queue->recv_fd(&fds);
            if (ret == 0 && fds.size() >0) {
                for (size_t i =0; i< fds.size(); ++i)
                { 
                    int fd=fds[i];
                    set_nodelay(fd);
                    set_none_block(fd);
                    ++this->data.cur_conn_num;

                    conn_info->server = this;

                    conn_info->fd = fd;

                    proc_pool(this, conn_info);

                    conn_info = this->conn_info_pool.alloc();
                }
            } else {
                break;
            }
        }
    }

    if (conn_info) {
        delete conn_info;
    }

    this->state = SERVER_STOPED;
    return 0;
}

int tcp_server_t::main_proc()
{
    
    // 不使用 fd pool 

    int listen_fd = this->listen_fd; 
    if (listen_fd <0) 
    {
        listen_fd = create_tcp_socket(this->port, this->ip.c_str(), true);
        if (listen_fd <0) 
        {
            this->state = SERVER_STOPED;
            PLOG_ERROR("create listen socket failed, "
                "[",this->ip,":",this->port,"]"
                "[errno:",errno,"]"
                "[errmsg:",strerror(errno),"]");
            return -1;
        }

//        enable_reuseport_cbpf(listen_fd);

        this->listen_fd = listen_fd;
    } 

    //设置为 非阻塞, accept 的时候就不会塞住 
    int ret = 0;
    ret = set_none_block(listen_fd);
    if (ret != 0)
    {
        PLOG_ERROR("set non block listen socket failed, "
            "[",this->ip,":",this->port,"]"
            "[fd=",listen_fd,"]"
            "[errno:",errno,"]"
            "[errmsg:",strerror(errno),"]");
        return -1;
    }

/*
    int busy_poll = 1000;
    ret = setsockopt(listen_fd, SOL_SOCKET, SO_BUSY_POLL, &busy_poll,sizeof(busy_poll));
    if (ret != 0)    {
        PLOG_ERROR("set busy poll in listen socket failed, "
            "[",this->ip,":",this->port,"]"
            "[fd=",listen_fd,"]"
            "[errno:",errno,"]"
            "[errmsg:",strerror(errno),"]");
    }
    */

    listen(listen_fd, this->conf.listen_backlog);

    if (FLAGS_enable_defer_accept) {
        int waits = 1; // 1 seconds;
        // 延迟 accept, 就是等客户端发了数据再accept 上套接字, 免去读等待
        ret = setsockopt(listen_fd, IPPROTO_IP, TCP_DEFER_ACCEPT, &waits, sizeof(waits));
        if (ret != 0)
        {
            PLOG_ERROR("set tcp_defer_accept socket option failed,"
                "[",this->ip,":",this->port,"]"
                "[fd=",listen_fd,"]"
                "[errno:",errno,"]"
                "[errmsg:",strerror(errno),"]");
        }
    }


    std::vector<int> new_fds;
    conn_info_t * conn_info = this->conn_info_pool.alloc();

    int accept_num = FLAGS_accept_num;
    while (0==this->to_stop)
    {
        while (this->data.cur_conn_num >= this->conf.max_conn_num)
        {
            usleep(10*1000); // block 10ms
            if (this->to_stop)
            {
                break;
            }
        }
        struct pollfd pf = { 0 };
        pf.fd = listen_fd;
        pf.events = (POLLIN|POLLERR|POLLHUP);
        ret = poll(&pf, 1, 1000);
        if (ret == 0)
        {
            continue;
        }
        if (ret <0) {
            if (errno == EINTR)
            {
                continue;
            }
            PLOG_ERROR("poll listen fd failed, ", (listen_fd, ret, errno), "[errmsg=", strerror(errno),"]");
            continue;
        }

        socklen_t len = sizeof(conn_info->addr);

        new_fds.clear();
        for(int i=0; i<accept_num; ++i)
        {
            int fd = conet::my_accept4(listen_fd, (struct sockaddr *)&conn_info->addr, &len, O_NONBLOCK);
            //int fd = accept(listen_fd, (struct sockaddr *)&conn_info->addr, &len);
            if (fd <0) break;
            new_fds.push_back(fd);
        }

        for (size_t i=0; i< new_fds.size(); ++i)
        {
            int fd = new_fds[i];
            set_nodelay(fd);
            ++this->data.cur_conn_num;
            //memset(conn_info, 0, sizeof(conn_info_t));

            conn_info->server = this;
            //memcpy(&conn_info->addr, &addr,len);

            conn_info->fd = fd;
            conn_info->extend = this->cb_arg;

            proc_pool(this, conn_info);
            conn_info = NULL;

            conn_info = this->conn_info_pool.alloc();
        }
    }

    PLOG_INFO("tcp server stoping, ready to clean up");

    if (conn_info) {
        this->conn_info_pool.release(conn_info);
        conn_info = NULL;
    }

    close(listen_fd);
    clean_up();
    this->state = SERVER_STOPED;
    return 0;
}

int tcp_server_t::clean_up()
{
    tcp_server_t *server = this;
    if (NULL == server->main_co)
    {
        PLOG_ERROR("tcp server stop by multi time");
        return 0;
    }

    if (this->exit_wait_ms >0) {
        for (int i=0; i< this->exit_wait_ms; ++i)
        {
            if (server->data.cur_conn_num <= 0) break;
            if (i%1000==0)
            {
                PLOG_INFO("wait server[",server->ip,":",server->port , "] conn exit");
            }
            usleep(1000);
        }
    }

    //this->co_pool.clear();

    server->state = SERVER_STOPED;

    if (server->data.cur_conn_num > 0) {
        PLOG_ERROR("server[",server->ip,":",server->port
            ,"] exit, but leak conn num:",server->data.cur_conn_num);
        return -1;
    }
    return 0;
}

}
