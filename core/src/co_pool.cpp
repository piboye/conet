/*
 * =====================================================================================
 *
 *       Filename:  co_pool.cpp
 *
 *    Description:
 *
 *        Version:  1.0
 *        Created:  2014年05月21日 04时12分06秒
 *       Revision:  none
 *       Compiler:  gcc
 *
 *         Author:  piboye
 *   Organization:
 *
 * =====================================================================================
 */
#include <stdlib.h>
#include "base/incl/list.h"
#include "co_pool.h"

namespace conet
{

void init_co_pool(co_pool_t *pool, int max_num)
{
    INIT_LIST_HEAD(&pool->free_list);
    INIT_LIST_HEAD(&pool->used_list);
    pool->total_num = 0;
    pool->max_num = max_num;
}

}
