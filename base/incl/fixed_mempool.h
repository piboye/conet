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

    void * alloc();
    void free(void *);
    int init(uint64_t alloc_size,  uint64_t max_num, int align_size = 0);
};

}


#endif /* end of include guard */
