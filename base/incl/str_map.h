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

#include "list.h"
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
        list_head link_to;

        void init(char const * str, size_t len) 
        {
            this->str = str;
            this->len = len;
            INIT_HLIST_NODE(&this->node);
            INIT_LIST_HEAD(&this->link_to);
        }
    };

    hlist_head *m_bucket;     
    size_t m_bsize; //bucket size
    size_t m_num;

    list_head m_list;

    StrMap() 
    {
        m_bucket = NULL;
        m_bsize =0;
        m_num = 0;
        INIT_LIST_HEAD(&m_list);
    }

    ~StrMap()  
    {
        if (m_bucket) {
            delete m_bucket;
        }
    }

    void init(int size) 
    {
        m_bucket = (hlist_head *) malloc(sizeof(hlist_head)*size);
        m_bsize = size;

        for (size_t i =0; i< m_bsize; ++i) 
        {
            INIT_HLIST_HEAD(&m_bucket[i]);
        }
    }

    size_t size() const {
        return m_num;
    }

    int add(StrNode * node)
    {
        size_t hash  = bob_hash(node->str, node->len, 1234);
        hlist_head *head = m_bucket + hash%m_bsize;
        hlist_add_head(&node->node, head);
        list_add(&node->link_to, &m_list);
        ++m_num;
        return 0;
    }

    int remove(StrNode *node)
    {
        hlist_del_init(&node->node);
        list_del_init(&node->link_to);
        --m_num;
        return 0;
    }

    StrNode * find(char const * str, size_t len)
    {
        size_t hash  = bob_hash(str, len, 1234);
        hlist_head *head = m_bucket + hash%m_bsize;
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
