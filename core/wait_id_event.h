/*
 * =====================================================================================
 *
 *       Filename:  wait_id_event.h
 *
 *    Description:  
 *
 *        Version:  1.0
 *        Created:  2014年11月03日 19时49分16秒
 *       Revision:  none
 *       Compiler:  gcc
 *
 *         Author:  piboye
 *   Organization:  
 *
 * =====================================================================================
 */

#ifndef __CONET_WAIT_ID_EVENT_H__
#define __CONET_WAIT_ID_EVENT_H__

#include "../../base/incl/int_map.h"
#include "coroutine.h"
#include "timewheel.h"

namespace conet 
{

struct WaitIdEvent
{
    IntMap wait_map;

    void init(int size) 
    {
        wait_map.init(size);
    }

    struct Node
    {
        IntMap::node_type map_node;
        uint64_t id;
        int timeout;
        coroutine_t *co;
        timeout_handle_t tm;
        void *msg;
    };

    static void timeout_proc(void *arg)
    {
        Node *node = (Node *)(arg);
        node->timeout = 1;
        conet::resume(node->co);
    }

    // 0 正常， 1 超时, -1 是id 已经存在
    int wait(uint64_t id, void **msg, int ms=0)
    {
        Node node;
        node.map_node.init(id);
        node.id = id;
        node.co = CO_SELF();
        node.timeout = 0;
        int ret = this->wait_map.add(&node.map_node);
        if (ret) {
           return -1; 
        }

        if (ms > 0) {
            init_timeout_handle(&node.tm, &timeout_proc, &node);
            set_timeout(&node.tm, ms);
        }
        conet::yield();
        if (ms >0) {
            cancel_timeout(&node.tm);
        }
        if (node.timeout) {
            return 1;
        }

        *msg = node.msg;
        return 0;
    }

    int notify(uint64_t id, void * msg)
    {
        IntMap::node_type *n1  = this->wait_map.find(id);

        if (NULL == n1 )  {
            return -1;
        }

        Node *node = container_of(n1, Node, map_node);
        node->msg = msg;
        conet::resume(node->co);
        return 0;
    }
};

}

#endif /* end of include guard */
