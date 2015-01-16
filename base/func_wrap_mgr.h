/*
 * =====================================================================================
 *
 *       Filename:  func_wrap_mgr.h
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

#ifndef __CONET_FUNC_WRAP_MGR_H__
#define __CONET_FUNC_WRAP_MGR_H__

namespace conet
{

struct FuncWrapData
{
    uint64_t jump_func;
    uint64_t self;
    uint64_t mem_func;
    uint64_t other;
    char code[32];
};

FuncWrapData * get_func_wrap_data();
void free_func_wrap_data(FuncWrapData *d);

inline
void free_func_wrap(void *f)
{
    free_func_wrap_data(
            (FuncWrapData *)((char*)f - ((size_t)&((FuncWrapData *)0)->code)));
}

}

#endif /* end of include guard */

