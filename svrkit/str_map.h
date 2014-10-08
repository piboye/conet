/*
 * =====================================================================================
 *
 *       Filename:  str_map.h
 *
 *    Description:  
 *
 *        Version:  1.0
 *        Created:  2014年10月08日 15时47分19秒
 *       Revision:  none
 *       Compiler:  gcc
 *
 *         Author:  piboyeliu
 *   Organization:  
 *
 * =====================================================================================
 */
#ifndef STR_MAP_H_INCL
#define STR_MAP_H_INCL

#include "core/incl/list.h"
#include "bobhash.h"

namespace conet
{

class StrMap
{
public:
    struct StrNode
    {
        char const * str;
        size_t len;
        hlist_node node;
        void init(char const * str, size_t len) 
        {
            this->str = str;
            this->len = len;
            INIT_HLIST_NODE(&this->node);
        }
    };

    hlist_head *m_bucket;     
    size_t m_size;

    StrMap() 
    {
        m_bucket = NULL;
        m_size =0;
    }

    void init(int size) 
    {
        m_bucket = (hlist_head *) malloc(sizeof(hlist_head)*size);
        m_size = size;

        for (size_t i =0; i< m_size; ++i) 
        {
            INIT_HLIST_HEAD(&m_bucket[i]);
        }
    }

    int add(StrNode * node)
    {
        size_t hash  = bob_hash(node->str, node->len, 1234);
        hlist_head *head = m_bucket + hash%m_size;
        hlist_add_head(&node->node, head);
        return 0;
    }

    int remove(StrNode *node)
    {
        hlist_del_init(&node->node);
        return 0;
    }

    StrNode * find(char const * str, size_t len)
    {
        size_t hash  = bob_hash(str, len, 1234);
        hlist_head *head = m_bucket + hash%m_size;
        hlist_node *pos = NULL;
        StrNode *node = NULL;
        hlist_for_each_entry(node, pos, head, node)
        {
            if (node->len != len) continue; 
            if (0 == memcmp(node->str, str, len)) {
                return node;
            }
        }
         
        return NULL;
    }

};

}

#endif /* end of include guard */
