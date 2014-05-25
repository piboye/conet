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
 *         Author:  piboyeliu
 *   Organization:
 *
 * =====================================================================================
 */
#ifndef __MEM_POOL_H_INC__
#define __MEM_POOL_H_INC__

struct mempool_t
{
    uint64_t total_num;
    uint64_t used_num;
    uint64_t max_num;
    uint64_t alloc_size;
    list_head free_list;
};

int init_mempool(mempool_t *mgr, uint64_t alloc_size,  uint64_t max_num)
{
    mgr->total_num = 0;
    mgr->used_num = 0;
    mgr->max_num = max_num;
    mgr->alloc_size = alloc_size;
    INIT_LIST_HEAD(&mrg->free_list);
}


#endif /* end of include guard */
