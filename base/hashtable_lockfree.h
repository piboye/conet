/*
 * =====================================================================================
 *
 *       Filename:  hashtable_lockfree.h
 *
 *    Description:  
 *
 *        Version:  1.0
 *        Created:  04/06/2015 04:41:31 AM
 *       Revision:  none
 *       Compiler:  gcc
 *
 *         Author:  piboye
 *   Organization:  
 *
 * =====================================================================================
 */

#ifndef __CONET_HASHTABLE_LOCK_FREE_H__
#define __CONET_HASHTABLE_LOCK_FREE_H__


namespace conet
{

//内容只允许添加， 不允许删除 
struct FixedHashTableLockFree
{
    struct node_t
    {
        node_t * next;
        uint64_t key;
        void *value;
    }

    node_t * bucket; 

    uint64_t bucket_num;

    int init(uint64_t num)
    {
        bucket = new node_t*[num];
        bucket_num = (uint64_t)num;
    }

    FixedHashTableLockFree()
    {
        bucket = NULL;
        bucket_num = 0;
    }

    inline
    uint64_t hash_code(uint64_t key)
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


    int find(uint64_t key);


    int add(node_t * node)
    {
        uint64_t key = node->key;
        uint64_t hash = hash_code(key);
        uint64_t pos = hash % bucket_num; 

        bucket + pos;

    }

};


}

#endif /* end of include guard */

