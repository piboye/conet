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

#include "hash_table_base.h"

namespace conet
{




class IntMap 
    : public HashTableBase<uint64_t, IntMap>
{
public:

static
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

};

}

#endif /* end of include guard */
