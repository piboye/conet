/*
 * =====================================================================================
 *
 *       Filename:  mem_pool.h
 *
 *    Description:
 *
 *        Version:  1.0
 *        Created:  2014年05月11日 04时39分46秒
 *       Revision:  none
 *       Compiler:  gcc
 *
 *         Author:  piboye
 *   Organization:
 *
 * =====================================================================================
 */
#ifndef __MEM_POOL_H_INC__
#define __MEM_POOL_H_INC__

#include <stdint.h>
#include "list.h"

namespace conet
{

struct fixed_mempool_t
{
    uint64_t alloc_size;
    uint64_t total_num;
    uint64_t used_num;
    uint64_t free_num;
    uint64_t max_num;
    int32_t align_size;
    list_head free_list;

    struct {
        unsigned int is_page_alloc:1;  
    };

    fixed_mempool_t()
    {
        alloc_size = 0;
        total_num = 0;
        used_num = 0;
        free_num = 0;
        max_num = 0;
        align_size = 0;
        INIT_LIST_HEAD(&free_list);
        is_page_alloc = 0;
    }

    void * alloc();
    void free(void *);
    int init(uint64_t alloc_size,  uint64_t max_num, int align_size = 0);

    void fini();

// help function
    void * alloc_mem_help();
    void free_mem_help(void *e);
};

template <typename T>
class FixedMempool
    :public fixed_mempool_t
{
public:
    typedef T value_type; 
    typedef fixed_mempool_t parent_type;

    explicit 
    FixedMempool(int max_num = 10000)
    {
        init(sizeof(value_type), max_num, __alignof__(value_type));
       
    }

    ~FixedMempool()
    {
        fini();
    }

    value_type *alloc()
    {
        value_type *v = new ((parent_type::alloc())) value_type;
        return v;
    }

    void free(value_type *v)
    {
        //delete (v) value_type;
        parent_type::free(v);
    }
};

}


#endif /* end of include guard */
