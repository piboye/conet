/*
 * =====================================================================================
 *
 *       Filename:  tcp_server.h
 *
 *    Description:
 *
 *        Version:  1.0
 *        Created:  2014年05月11日 07时43分57秒
 *       Revision:  none
 *       Compiler:  gcc
 *
 *         Author:  piboye
 *   Organization:
 *
 * =====================================================================================
 */
#ifndef __TCP_SERVER_H_INC__
#define __TCP_SERVER_H_INC__
#include <string>
#include <stdint.h>
#include <netinet/in.h>
#include "../core/conet_all.h"
#include "../core/co_pool.h"
#include "../base/list.h"
#include "../base/obj_pool.h"
#include "../base/unix_socket_send_fd.h"
#include "conn_info.h"
#include "server_base.h"

namespace conet
{

struct coroutine_t;

struct tcp_server_t: public ServerBase
{
    enum {
        SERVER_START=0,
        SERVER_RUNNING=1,
        SERVER_STOPED=2,
    };

    int listen_fd;
    std::string ip;
    int port;

    coroutine_t *main_co;
    int state;
    int to_stop;
    void *extend;

    typedef int (*conn_proc_cb_t)(void *arg, conn_info_t *conn);

    conn_proc_cb_t conn_proc_cb;
    void *cb_arg;

    obj_pool_t co_pool;

    ObjPool<conn_info_t> conn_info_pool;


    struct conf_t 
    {
        int listen_backlog;
        int max_packet_size;
        uint32_t max_conn_num;
    } conf;

    struct data_t
    {
        uint32_t cur_conn_num;
    } data;


    UnixSocketSendFd *accept_fd_queue;


    void set_conn_cb(conn_proc_cb_t cb, void *arg)
    {
        conn_proc_cb = cb;
        cb_arg = arg;
    }


    int main_proc();

    int main_proc2();
    int main_proc_with_fd_queue();
    

    int init(const char *ip, int port, int listen_fd=-1);

    virtual
    int start();

    virtual
    int stop(int wait_ms=0);

    ~tcp_server_t()
    {

    }
};

}

#endif /* end of include guard */

