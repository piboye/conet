/*
 * =====================================================================================
 *
 *       Filename:  obj_pool.h
 *
 *    Description:  
 *
 *        Version:  1.0
 *        Created:  2014年10月26日 22时40分33秒
 *       Revision:  none
 *       Compiler:  gcc
 *
 *         Author:  piboye
 *   Organization:  
 *
 * =====================================================================================
 */

#ifndef __PB_OBJ_POOL_H_INC__
#define __PB_OBJ_POOL_H_INC__

#include "google/protobuf/message.h"
#include "lifo_lockfree.h"

namespace conet
{

class PbObjPool
{
public:
    google::protobuf::Message * m_obj_proto;    
    
    lifo_lockfree_t m_queue;
    int m_hold_proto_flag;

    typedef lifo_lockfree_t::node_t node_t;

    explicit
    PbObjPool()
    {
        m_hold_proto_flag = 0;
        m_obj_proto = NULL;
    }

    ~PbObjPool() 
    {
        node_t *n = NULL;
        while (1)
        {
            n = m_queue.pop();
            if (n) {
                delete (google::protobuf::Message *)(n->value);
                m_queue.free_node(n);
            } else {
                break;
            }
        }

        if (m_hold_proto_flag == 1) {
            delete m_obj_proto;
        }
    }


    int init(google::protobuf::Message *pb, int hold=0)
    {
        m_queue.init();
        if (m_hold_proto_flag && m_obj_proto) {
            delete m_obj_proto;
        }

        m_obj_proto = pb;
        m_hold_proto_flag = hold;

        return 0;
    }

    int alloc(PbObjPool::node_t **o_node, google::protobuf::Message **o_msg) 
    {
        node_t *n = NULL;
        n = m_queue.pop();
        if (NULL == n) 
        {
            n = m_queue.alloc_node();
        }

        if (NULL == n->value) {
            n->value = m_obj_proto->New();
        } 

        *o_msg = (google::protobuf::Message *)((n)->value);
        *o_node = n;
        return 0;
    }

    void release(PbObjPool::node_t *node, google::protobuf::Message *value)
    {
        m_queue.push(node, value);
    }

};

google::protobuf::Message * alloc_pb_obj_from_pool(google::protobuf::Message *proto);

void free_pb_obj_to_pool(google::protobuf::Message *proto, google::protobuf::Message *obj);

}

#endif /* end of include guard */
