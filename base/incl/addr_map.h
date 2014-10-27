/*
 * =====================================================================================
 *
 *       Filename:  addr_map.h
 *
 *    Description:  
 *
 *        Version:  1.0
 *        Created:  2014年10月27日 15时22分03秒
 *       Revision:  none
 *       Compiler:  gcc
 *
 *         Author:  piboye
 *   Organization:  
 *
 * =====================================================================================
 */
#ifndef __CONET_ADDR_MAP_H__
#define __CONET_ADDR_MAP_H__

#include "hash_table_base.h"

namespace conet
{


class AddrMap : public HashTableBase<void *, AddrMap>
{
public:

static
inline
uint64_t hash_code(void* addr)
{ 
  return  (uint64_t)(addr) * 2654435761;
}

static inline 
bool equal(void *l, void *r)
{
    return l == r;
}

};

}


#endif /* end of include guard */
