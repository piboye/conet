/*
 * =====================================================================================
 *
 *       Filename:  channel.h
 *
 *    Description:  
 *
 *        Version:  1.0
 *        Created:  2015年03月09日 11时27分02秒
 *       Revision:  none
 *       Compiler:  gcc
 *
 *         Author:  piboye
 *   Organization:  
 *
 * =====================================================================================
 */

#ifndef __CONET_CHANNEL_T_H__
#define __CONET_CHANNEL_T_H__
#include "conn_info.h"
#include "core/coroutine.h"
#include "base/list.h"
#include "server_base.h"
#include "base/obj_pool.h"
#include "core/wait_queue.h"

namespace conet
{

struct channel_t
{
// data
    conn_info_t *conn;
    server_base_t *server;

    struct tx_data_t
    {
        list_head link_to;
        void *data;
        int len;

        void (*free_fn)(void *arg, void *data, int len);
        void * free_arg;
        void init(void *data, int len,
                void (*free_fn)(void*arg, void *data, int len) = NULL,
                void *arg = NULL
                )
        {
            INIT_LIST_HEAD(&link_to);
            this->data = data;
            this->len = len;
            this->free_fn = free_fn;
            this->free_arg = arg;
        }
    };

    // 读缓存
    char *read_buff;
    size_t read_buff_len;
    typedef int(*new_data_cb_t)(void *arg, char const * data, int len);
    new_data_cb_t new_data_cb;
    void * new_data_arg;

    // 设置新数据回调
    int set_new_data_cb( new_data_cb_t cb, void *arg);

    // 发送数据队列
    list_head tx_queue;
    ObjPool<tx_data_t> tx_data_pool;
    int send_msg(char const *data, int len,
                void (*free_fn)(void*arg, void *data, int len) = NULL,
                void *arg = NULL
            );

    int to_stop;
    int r_stop;
    int w_stop;

    // 发送数据等待通知
    conet::WaitQueue write_waiter;

    coroutine_t * r_co;
    coroutine_t * w_co;

// interface:
//
    int init(conn_info_t *conn, server_base_t* server);
    int start();
    int stop();
    channel_t();
    ~channel_t();

// private function
//
    static int do_read_co(void *arg);

    static int do_write_co(void *arg);

};

}

#endif /* end of include guard */
