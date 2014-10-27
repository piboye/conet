/*
 * =====================================================================================
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
#ifndef STR_MAP_H_INCL
#define STR_MAP_H_INCL

#include "hash_table_base.h"
#include "murmurhash3.h"
#include "ref_str.h"

namespace conet
{

class StrMap
    :public HashTableBase<ref_str_t, StrMap> 
{
public:
    static inline
    uint64_t hash_code(ref_str_t const &key)
    {
        return MurmurHash64A(key.data, key.len, 0);
    }

    static inline
    bool equal(ref_str_t const &l, ref_str_t const &r)
    {
       return conet::equal(l, r);
    }

};

}

#endif /* end of include guard */
