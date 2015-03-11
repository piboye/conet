/*
 * =====================================================================================
 *
 *       Filename:  udp_server.h
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
#ifndef __UDP_SERVER_H_INC__
#define __UDP_SERVER_H_INC__
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
#include "../core/wait_queue.h"

namespace conet
{

struct coroutine_t;

struct udp_server_t: public server_base_t
{

    int udp_socket;
    int write_fd;

    std::string ip;
    int port;

    coroutine_t *main_co;
    // 数据发送协程
    coroutine_t *tx_co;

    void *extend;

    struct udp_req_ctx_t
    {
        conn_info_t conn_info;
        char * data;
        size_t len;
    };

    struct tx_data_t
    {
        list_head link_to;
        void *data;
        int len;
        sockaddr dst_addr;
        void init(void *data, int len, sockaddr const *addr)
        {
            INIT_LIST_HEAD(&link_to);
            this->data = data;
            this->len = len;
            memcpy(&this->dst_addr, addr, sizeof(*addr));
        }
    };

    // 发送数据等待通知
    conet::WaitQueue rsp_wait;

    ObjPool<tx_data_t> tx_data_pool;

    list_head tx_queue;


    typedef int (*conn_proc_cb_t)(void *arg, conn_info_t *conn, 
            char const * data, size_t len,
            char * out_data, size_t *olen
            );

    conn_proc_cb_t conn_proc_cb;
    void *cb_arg;
    void set_conn_cb(conn_proc_cb_t cb, void *arg)
    {
        conn_proc_cb = cb;
        cb_arg = arg;
    }

    obj_pool_t co_pool;

    ObjPool<udp_req_ctx_t> udp_req_pool;

    fixed_mempool_t buffer_pool; 

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


    int main_proc();

    int main_proc2();

    int init(const char *ip, int port, int fd=-1);

    virtual
    int start();

    virtual int do_stop(int wait_ms);

    ~udp_server_t()
    {
        buffer_pool.fini();
    }
};

}

#endif /* end of include guard */

