/*
 * =====================================================================================
 *
 *       Filename:  bind_this_mgr.h
 *
 *    Description:
 *
 *        Version:  1.0
 *        Created:  01/14/2015 09:50:55 AM
 *       Revision:  none
 *       Compiler:  gcc
 *
 *         Author:  piboye
 *   Organization:
 *
 * =====================================================================================
 */

#ifndef __CONET_BIND_THIS_MGR_H__
#define __CONET_BIND_THIS_MGR_H__

#include <stdint.h>
#include <unistd.h>

namespace conet
{

struct BindThisData
{
    uint64_t jump_func;
    uint64_t self;
    uint64_t mem_func;
    uint64_t other;
    char code[32];
};

BindThisData * get_bind_this_data();
void free_bind_this_data(BindThisData *d);

inline
void free_bind_this_func(void *f)
{
    free_bind_this_func(
            (BindThisData *)((char*)f - ((size_t)&((BindThisData *)0)->code)));
}

}

#endif /* end of include guard */

