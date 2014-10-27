/*
 * =====================================================================================
 *
 *       Filename:  hash_table_base.h
 *
 *    Description:  
 *
 *        Version:  1.0
 *        Created:  2014年10月27日 15时57分29秒
 *       Revision:  none
 *       Compiler:  gcc
 *
 *         Author:  piboye
 *   Organization:  
 *
 * =====================================================================================
 */
#ifndef __CONET__HASH_TABLE_BASE_H__
#define __CONET__HASH_TABLE_BASE_H__

#include "list.h"
#include "murmurhash3.h"

namespace conet
{

template<typename ValueType, typename ParentType>
class HashTableBase
{
public:
    typedef ValueType value_type; 
    typedef ParentType parent_type;
    typedef HashTableBase<ValueType, ParentType> hash_base_type;

    struct Node {
        value_type value;
        hlist_node node;
        list_head link_to;

        void init(value_type const & val)
        {
            this->value = val;
            INIT_HLIST_NODE(&this->node);
            INIT_LIST_HEAD(&this->link_to);
        }
    };

    typedef Node node_type;

    int (*m_destructor_func)(void *arg, Node *node);
    void *m_destructor_arg;

    hlist_head *m_bucket;     
    size_t m_bsize; //bucket size
    size_t m_num;

    list_head m_list;

    HashTableBase() 
    {
        m_bucket = NULL;
        m_bsize =0;
        m_num = 0;
        INIT_LIST_HEAD(&m_list);
        m_destructor_func = NULL;
        m_destructor_arg = NULL;
    }

    ~HashTableBase()  
    {
        if (m_destructor_func) {
            Node *item=NULL, *next = NULL;
            list_for_each_entry_safe(item, next, &m_list, link_to)
            {
               m_destructor_func(m_destructor_arg, item); 
            }
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

    void set_destructor_func(int (*fini_func)(void *, Node *), void *arg)
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
        uint64_t hash  = parent_type::hash_code(node->value);
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

    Node * find(value_type const &value)
    {
        uint64_t hash  = parent_type::hash_code(value);
        hlist_head *head = m_bucket + hash%m_bsize;
        hlist_node *pos = NULL;
        Node *node = NULL;
        hlist_for_each_entry(node, pos, head, node)
        {
            if (parent_type::equal(node->value,value)) {
                return node;
            }
        }
         
        return NULL;
    }

    static 
    inline
    bool equal(value_type const & l, value_type const &r) 
    {
        return l == r;
    }

    static  
    inline 
    uint64_t hash_code(value_type const &key)
    {
        return MurmurHash64A(&key, sizeof(key), 0);
    }

};

}


#endif /* end of include guard */
