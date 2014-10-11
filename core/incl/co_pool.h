/*
 * =====================================================================================
 *
 *       Filename:  co_pool.h
 *
 *    Description:  
 *
 *        Version:  1.0
 *        Created:  2014年06月25日 10时39分09秒
 *       Revision:  none
 *       Compiler:  gcc
 *
 *         Author:  piboyeliu
 *   Organization:  
 *
 * =====================================================================================
 */
#ifndef CO_POLL_H_INC
#define CO_POLL_H_INC

#include "conet_all.h"
namespace conet
{

struct co_pool_item_t
{
    conet::coroutine_t *co;
    list_head link;
};

struct co_pool_t
{
    list_head free_list;
    list_head used_list;
    int total_num;
    int max_num;
};

void init_co_pool(co_pool_t *pool, int max_num);

}

#endif /* end of include guard */
