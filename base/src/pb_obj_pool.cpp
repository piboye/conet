/*
 * =====================================================================================
 *
 *       Filename:  pb_obj_pool.cpp
 *
 *    Description:  
 *
 *        Version:  1.0
 *        Created:  2014年10月27日 12时53分20秒
 *       Revision:  none
 *       Compiler:  gcc
 *
 *         Author:  piboye
 *   Organization:  
 *
 * =====================================================================================
 */
#include <stdlib.h>
#include "pb_obj_pool.h"
#include "tls.h"
#include "obj_pool.h"
#include "addr_map.h"

namespace conet
{
    struct PbObjPoolMgr
    {
        typedef ObjPool<google::protobuf::Message> pb_obj_pool_type;
        
        struct PbObjPoolNode
        {
            AddrMap::Node map_node;

            obj_pool_t obj_pool;

            static int fini(void *arg, AddrMap::Node *n)
            {
                PbObjPoolNode * node = container_of(n, PbObjPoolNode, map_node);
                delete node; 
                return 0;
            }
        };

        AddrMap pool_map;

        PbObjPoolMgr()
        {
            pool_map.init(100);
            pool_map.set_destructor_func(&PbObjPoolNode::fini, this);
        }

        ~PbObjPoolMgr()
        {

        }

        static 
        void *pb_new_obj(void * arg)
        { 
            google::protobuf::Message *proto = (google::protobuf::Message *)(arg);
            return proto->New();
        }
        static void pb_delete_obj(void *arg, void *obj)
        {
            delete (google::protobuf::Message *)(obj);
        }

        obj_pool_t * find(google::protobuf::Message *proto) 
        {
            AddrMap::Node * n = pool_map.find(proto);
            PbObjPoolNode * node = NULL;
            if (n == NULL) 
            {
                node = new PbObjPoolNode ();
                node->map_node.init(proto);
                node->obj_pool.set_alloc_obj_func(pb_new_obj, proto);
                node->obj_pool.set_free_obj_func(pb_delete_obj, proto);
                pool_map.add(&node->map_node);
            } else {
                node = container_of(n, PbObjPoolNode, map_node);
            }
            return &node->obj_pool;
        }

    };

__thread PbObjPoolMgr * g_pb_obj_pool_mgr = NULL;

CONET_DEF_TLS_VAR_HELP(g_pb_obj_pool_mgr, 
        ({
            new PbObjPoolMgr();
        }),
        ({
            delete self;
        })
);


google::protobuf::Message * alloc_pb_obj_from_pool(google::protobuf::Message *proto)
{
    PbObjPoolMgr * mgr = TLS_GET(g_pb_obj_pool_mgr);
    obj_pool_t *pool = mgr->find(proto);
    return (google::protobuf::Message *) pool->alloc();
}

void free_pb_obj_to_pool(google::protobuf::Message *proto, google::protobuf::Message *obj)
{
    PbObjPoolMgr * mgr = TLS_GET(g_pb_obj_pool_mgr);
    obj_pool_t *pool = mgr->find(proto);
    return pool->release(obj);
}

}
