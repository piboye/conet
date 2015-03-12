/*
 * =====================================================================================
 *
 *       Filename:  channel.cpp
 *
 *    Description:  
 *
 *        Version:  1.0
 *        Created:  2015年03月09日 14时24分41秒
 *       Revision:  none
 *       Compiler:  gcc
 *
 *         Author:  piboye
 *   Organization:  
 *
 * =====================================================================================
 */
#include "channel.h"
#include "base/net_tool.h"
#include "thirdparty/glog/logging.h"

#include <errno.h>
#include <stdlib.h>

namespace conet
{

channel_t::channel_t()
{
    conn = NULL;
    server = NULL;
    read_buff_len = 0;
    read_buff = NULL;
    to_stop = 0;
    r_stop = 0;
    w_stop = 0;
    new_data_cb = NULL;
    new_data_arg = NULL;
    r_co = NULL;
    w_co = NULL;

    INIT_LIST_HEAD(&tx_queue);
    INIT_LIST_HEAD(&link_to);
}

channel_t::~channel_t()
{
    if (read_buff)
    {
        delete read_buff;
    }

    tx_data_t * out_data= NULL, *n = NULL;
    list_for_each_entry_safe(out_data, n, &tx_queue, link_to)
    {
        if (out_data->free_fn)
        {
            out_data->free_fn(out_data->free_arg, out_data->data, out_data->len);
        }
        list_del(&out_data->link_to);
        tx_data_pool.release(out_data);
    }
}

int channel_t::init(conn_info_t *conn, server_base_t *server)
{
    this->conn = conn;
    this->server = server;
    return 0;
}

int channel_t::send_msg(char const *data, int len, 
        void (*free_fn)(void*arg, void *data, int len),
        void *free_arg
)
{
    tx_data_t *tx_data = tx_data_pool.alloc();
    tx_data->init((void *)data, len, free_fn, free_arg); 
    list_add_tail(&tx_data->link_to, &tx_queue);
    write_waiter.wakeup_all();
    return 0;
}

int channel_t::set_new_data_cb( new_data_cb_t cb, void *arg)
{
    new_data_cb = cb;
    new_data_arg = arg;
    return 0;
}

int channel_t::do_read_co(void *arg)
{
    int ret = 0;
    channel_t *self = (channel_t *)(arg);
    int fd = dup(self->conn->fd);
    char * read_buff = self->read_buff;
    int max_len = self->read_buff_len;

    new_data_cb_t cb = self->new_data_cb;
    void * cb_arg= self->new_data_arg;

    while ( 0 == self->to_stop && 0 == self->w_stop)
    {
        struct pollfd pf = { 0 };
        pf.fd = fd;
        pf.events = (POLLIN|POLLERR|POLLHUP);
        ret = poll(&pf, 1, 1000);
        if (ret == 0) continue;
        if (ret <0) {
            if (errno == EINTR) {
                continue;
            }
            self->r_stop = 1;
            LOG(ERROR)<<"pool error ret:"<<ret;
            break;
        }
        ret = recv(fd, read_buff, max_len, 0); 
        if (ret < 0)
        {
            LOG(ERROR)<<"read data failed, ret:"<<ret;
            break;
        }
        if (ret == 0) 
        { // close by peer
            LOG(ERROR)<<"close by peer";
            break;
        }

        if ( cb)
        {
            ret = cb(cb_arg, read_buff, ret);
            if (ret < 0)
            {  // 出错了
                self->to_stop = 1;
                break;
            } 
            else if (ret == 0)
            { // 关闭连接
                self->to_stop = 1;
                break;
            }
        }
    }

    self->r_stop = 1;
    self->write_waiter.wakeup_all();
    close(fd);
    return 0;
}

int channel_t::do_write_co(void *arg)
{
    int ret = 0;
    channel_t *self = (channel_t *)(arg);
    int fd = dup(self->conn->fd);
    while (self->to_stop == 0 && self->r_stop == 0)
    {
        list_head queue;
        INIT_LIST_HEAD(&queue);
        list_swap(&queue, &self->tx_queue);

        if (list_empty(&queue))
        {
            self->write_waiter.wait_on();
            continue;
        }

        tx_data_t * out_data= NULL, *n = NULL;
        list_for_each_entry_safe(out_data, n, &queue, link_to)
        {
            ret = send(fd, out_data->data, out_data->len, 0);
            if (out_data->free_fn)
            {
                out_data->free_fn(out_data->free_arg, out_data->data, out_data->len);
            }
            list_del(&out_data->link_to);
            self->tx_data_pool.release(out_data);

            if (ret<0)
            {
                LOG(ERROR)<<"channel send data failed!"
                        "[fd:"<<fd<<"]"
                        "[data len:"<<out_data->len<<"]"
                        "[ret:"<<ret<<"]"
                        ;
            }
        }
    }

    close(fd);
    self->w_stop = 1;
    self->exit_notify.wakeup_all();
    return 0;
}

int channel_t::start()
{
    if (read_buff_len == 0)
    {
        read_buff_len = 10240;
    }

    read_buff = new char[read_buff_len];

    if (NULL == r_co)
    {
        r_co = conet::alloc_coroutine(do_read_co, this);
        conet::resume(r_co, this);
    }

    if (NULL == w_co)
    {
        w_co = conet::alloc_coroutine(do_write_co, this);
        conet::resume(w_co, this);
    }

    return 0;
}


int channel_t::stop()
{
    to_stop = 1;

    if (r_co)
    {
        conet::wait(r_co);
        conet::free_coroutine(r_co);
        r_co = NULL;
    }

    if (w_co)
    {
        conet::wait(w_co);
        conet::free_coroutine(w_co);
        w_co = NULL;
    }

    return 0;
}






}

