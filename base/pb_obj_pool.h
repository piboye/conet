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
#include "obj_pool.h"

namespace conet
{

class PbObjPool
{
public:
    google::protobuf::Message * m_obj_proto;
    google::protobuf::Arena arena;
    
    obj_pool_t m_queue;
    int m_hold_proto_flag;

    explicit
    PbObjPool()
    {
        m_hold_proto_flag = 0;
        m_obj_proto = NULL;
    }

    ~PbObjPool() 
    {
        if (m_hold_proto_flag == 1) {
            delete m_obj_proto;
        }
    }

    static 
    void * pb_obj_new(void *arg) 
    {
       PbObjPool * self = (PbObjPool *)(arg);
       return self->m_obj_proto->New(&self->arena);
    }

    static 
    void pb_obj_free(void *arg, void *obj) 
    {
        //google::protobuf::Message * msg = (google::protobuf::Message *)(obj);
        //delete msg;
    }

    int init(google::protobuf::Message *pb, int hold=0)
    {
        if (m_hold_proto_flag && m_obj_proto) {
            delete m_obj_proto;
        }

        m_obj_proto = pb;

        m_hold_proto_flag = hold;

        m_queue.set_alloc_obj_func(&pb_obj_new, this);
        m_queue.set_free_obj_func(&pb_obj_free, this);

        return 0;
    }

    google::protobuf::Message * alloc()
    {
        return (google::protobuf::Message *) m_queue.alloc(); 
    }

    void release(google::protobuf::Message *value)
    {
        return m_queue.release(value);
    }

};

google::protobuf::Message * alloc_pb_obj_from_pool(google::protobuf::Message *proto);

void free_pb_obj_to_pool(google::protobuf::Message *proto, google::protobuf::Message *obj);

}

#endif /* end of include guard */
