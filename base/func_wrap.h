/*
 * =====================================================================================
 *
 *       Filename:  func_wrap.h
 *
 *    Description:  
 *
 *        Version:  1.0
 *        Created:  2015年01月13日 15时41分17秒
 *       Revision:  none
 *       Compiler:  gcc
 *
 *         Author:  piboye
 *   Organization:  
 *
 * =====================================================================================
 */
#ifndef __CONET_FUNC_WRAP_H__
#define __CONET_FUNC_WRAP_H__
#include <stdint.h>
#include <stdlib.h>

namespace conet
{
    template<typename R, typename T1>
        inline
        R func_wrap_pb100(T1 t1)
        {
            void * self=NULL;
            R (* func)(void *, T1 )=NULL;
            asm("movq %%r13, %0" :"=r"(self)::);
            asm("movq %%r14, %0" :"=r"(func)::);
            return func(self, t1);
        }

    struct FuncWrapData
    {
        uint64_t jump_func;
        uint64_t self;
        uint64_t mem_func;
        uint64_t other;
        char code[32];
    };
}

#endif /* end of include guard */

