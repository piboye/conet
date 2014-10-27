/* * =====================================================================================
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
#ifndef INT_MAP_H_INCL
#define INT_MAP_H_INCL

#include "list.h"
#include "bobhash.h"

namespace conet
{

inline
uint64_t address_hash(uint64_t addr)
{ 
  return  addr * 2654435761;
}

inline
uint64_t hash64shift(uint64_t key) 
{ 
  key = (~key) + (key << 21); // key = (key << 21) - key - 1; 
  key = key ^ (key >> 24); 
  key = (key + (key << 3)) + (key << 8); // key * 265 
  key = key ^ (key >> 14); 
  key = (key + (key << 2)) + (key << 4); // key * 21 
  key = key ^ (key >> 28); 
  key = key + (key << 31); 
  return key; 
}


class IntMap
{
public:

    struct Node
    {
        uint64_t key;
        hlist_node node;
        list_head link_to;

        void init(uint64_t key) 
        {
            this->key = key;
            INIT_HLIST_NODE(&this->node);
            INIT_LIST_HEAD(&this->link_to);
        }
    };

    int (*m_destructor_func)(Node *node, void *arg);
    void *m_destructor_arg;

    hlist_head *m_bucket;     
    size_t m_bsize; //bucket size
    size_t m_num;

    list_head m_list;

    IntMap() 
    {
        m_bucket = NULL;
        m_bsize =0;
        m_num = 0;
        INIT_LIST_HEAD(&m_list);
        m_destructor_func = NULL;
        m_destructor_arg = NULL;
    }

    ~IntMap()  
    {
        if (m_destructor_func) {
            foreach(m_destructor_func, m_destructor_arg);
        }
        if (m_bucket) {
            free(m_bucket);
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

    void set_destructor_func(int (*fini_func)(Node *, void *), void *arg)
    {
        m_destructor_func = fini_func;
        m_destructor_arg = arg;
    }

    size_t size() const 
    {
        return m_num;
    }

    int foreach(int (*proc)(Node *, void *arg), void *arg)
    {
       int cnt = 0;
       int ret = 0;
       Node *item=NULL, *next = NULL;
       list_for_each_entry_safe(item, next, &m_list, link_to)
       {
            ret = proc(item, arg);
            ++cnt;
       }
       return cnt; 
    }

    int add(Node * node)
    {
        size_t hash  = hash64shift(node->key);
        hlist_head *head = m_bucket + hash%m_bsize;
        hlist_add_head(&node->node, head);
        list_add(&node->link_to, &m_list);
        ++m_num;
        return 0;
    }

    int remove(Node *node)
    {
        hlist_del_init(&node->node);
        list_del_init(&node->link_to);
        --m_num;
        return 0;
    }

    Node * find(uint64_t key)
    {
        size_t hash  = hash64shift(key);
        hlist_head *head = m_bucket + hash%m_bsize;
        hlist_node *pos = NULL;
        Node *node = NULL;
        hlist_for_each_entry(node, pos, head, node)
        {
            if (node->key == key) {
                return node;
            }
        }
         
        return NULL;
    }

};

}

#endif /* end of include guard */
