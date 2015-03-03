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
#include "func_wrap_mgr.h"
#include "list.h"

namespace conet
{

template<typename ClassT, typename R>
inline
R (*get_func_wrap_helper( R (ClassT::*f)() ))()
{
    struct  Wrap
    {
        static R run()
        {
            void * self=NULL;
            R (* func)(void *)=NULL;
            asm("movq %%r13, %0" :"=r"(self)::);
            asm("movq %%r14, %0" :"=r"(func)::);
            return func(self);
        }
    };
    return &Wrap::run;
}

template<typename ClassT, typename R, typename T1>
inline
R (*get_func_wrap_helper( R (ClassT::*f)(T1) ))(T1)
{
    struct  Wrap
    {
        static R run(T1 t1)
        {
            void * self=NULL;
            R (* func)(void *, T1 )=NULL;
            asm("movq %%r13, %0" :"=r"(self)::);
            asm("movq %%r14, %0" :"=r"(func)::);
            return func(self, t1);
        }
    };
    return &Wrap::run;
}


#define BindThis(obj, func) \
    ({ \
        FuncWrapData *d = get_func_wrap_data();  \
        d->jump_func = (uint64_t)(get_func_wrap_helper(&func)); \
        d->self = (uint64_t )(&obj); \
        typeof(&func) pfn = &func; \
        memcpy(&d->mem_func, (void const *)(&pfn), sizeof(uint64_t)); \
        (typeof(get_func_wrap_helper(&func))) (&(d->code)); \
     })

}

#endif /* end of include guard */

