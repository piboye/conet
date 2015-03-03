/*
 * =====================================================================================
 *
 *       Filename:  udp_server.cpp
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
#include "udp_server.h"
#include "gflags/gflags.h"
#include "glog/logging.h"

#include "base/net_tool.h"
#include "base/ptr_cast.h"
#include "core/fd_ctx.h"


namespace conet
{

static
int conn_proc_co(udp_server_t::udp_req_ctx_t *req)
{
    conet::enable_sys_hook();
    conet::enable_pthread_hook();
    conet::coroutine_t *co = CO_SELF();
    req = (udp_server_t::udp_req_ctx_t *)yield();
    if (req == NULL)
    {
        return 0;
    }

    udp_server_t *server = (udp_server_t *)req->conn_info.server;

    udp_server_t::conn_proc_cb_t conn_proc = server->conn_proc_cb;
    void * cb_arg = server->cb_arg;
    int max_packet_size = server->conf.max_packet_size;
    do
    {   //running;

        int ret = 0;
        char *data = req->data;
        size_t len = req->len;

        char * obuffer = (char *)server->buffer_pool.alloc();
        size_t olen = max_packet_size;
        ret = conn_proc(cb_arg, &req->conn_info, data, len, obuffer,  &olen);
        
        if ( ret== 0 && olen > 0) {
            ret = sendto(server->udp_socket, obuffer, olen, 0, 
                (sockaddr *) &req->conn_info.addr, sizeof(req->conn_info.addr));
        }

        co = req->conn_info.co;
        --server->data.cur_conn_num;
        server->udp_req_pool.release(req);
        server->buffer_pool.free(data);
        server->buffer_pool.free(obuffer);

        req = NULL;

        if (ret) break;
        if (server->to_stop) break;

        server->co_pool.release(co);
        req = (udp_server_t::udp_req_ctx_t *) conet::yield(NULL);
    } while(req && !server->to_stop);

    if (req) {
        --server->data.cur_conn_num;
        server->udp_req_pool.release(req);
        req = NULL;
    }
    return 0;
}



static
int proc_pool(udp_server_t *server, udp_server_t::udp_req_ctx_t *req)
{
    req->conn_info.co = (coroutine_t *)server->co_pool.alloc();
    conet::resume(req->conn_info.co, req);
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

int udp_server_t::init(const char *ip, int port, int fd)
{
    udp_server_t *server = this;
    server->ip = ip;
    server->port = port;
    server->state = SERVER_START;
    server->main_co = NULL;
    server->extend = NULL;

    server->conf.max_conn_num = 10000;
    server->conf.max_packet_size = 1472;
    server->data.cur_conn_num = 0;
    server->to_stop = 0;
    server->udp_socket = fd;
    server->co_pool.set_alloc_obj_func(alloc_server_work_co, server);
    server->co_pool.set_free_obj_func(free_server_work_co, server);

    server->buffer_pool.init(server->conf.max_packet_size, server->conf.max_conn_num);
    return 0;
}



int udp_server_t::start()
{
    this->main_co = alloc_coroutine(conet::ptr_cast<co_main_func_t>(&udp_server_t::main_proc), this);
    conet::resume(this->main_co);
    return 0;
}


int udp_server_t::main_proc()
{
    conet::enable_sys_hook();
    conet::enable_pthread_hook();

    this->state = udp_server_t::SERVER_RUNNING;

    return this->main_proc2();
}


int udp_server_t::main_proc2()
{

    int  udp_socket = create_udp_socket(this->port, this->ip.c_str(), true);
    if (udp_socket <0) 
    {
        this->state = SERVER_STOPED;
        LOG(ERROR)<<"create listen socket failed, "
            "["<<this->ip<<":"<<this->port<<"]"
            "[errno:"<<errno<<"]"
            "[errmsg:"<<strerror(errno)<<"]";
        return -1;
    }
    this->udp_socket = udp_socket;

    set_none_block(udp_socket, true);

    int ret = 0;

    std::vector<int> new_fds;
    udp_req_ctx_t * req = this->udp_req_pool.alloc();
    int max_packet_size = this->conf.max_packet_size;
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
        pf.fd = udp_socket;
        pf.events = (POLLIN|POLLERR|POLLHUP);
        ret = co_poll(&pf, 1, 1000);
        if (ret == 0) continue;
        if (ret <0) {
            if (errno == EINTR) {
                continue;
            }
            break;
        }

        socklen_t len = sizeof(req->conn_info.addr);

        char *buffer = (char *)this->buffer_pool.alloc();
        ret = recvfrom(udp_socket, buffer, max_packet_size, O_NONBLOCK, 
                (sockaddr *)&req->conn_info.addr, &len); 

        ++this->data.cur_conn_num;

        req->conn_info.server = this;

        req->conn_info.fd = udp_socket;
        req->data = buffer;
        req->len = ret;

        proc_pool(this, req);

        req = this->udp_req_pool.alloc();
    }

    if (req) {
        delete req;
    }

    close(udp_socket);
    this->state = SERVER_STOPED;
    return 0;
}

int udp_server_t::stop(int wait_ms)
{
    udp_server_t *server = this;

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
