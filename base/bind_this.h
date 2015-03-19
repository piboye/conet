/*
 * =====================================================================================
 *
 *       Filename:  bind_this.h
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
#ifndef __CONET_BIND_THIS_H__
#define __CONET_BIND_THIS_H__
#include "bind_this_mgr.h"
#include "list.h"
#include <stdint.h>
#include <stdlib.h>
#include "macro_help.h"

namespace conet
{

template<typename ClassT, typename R>
inline
R (*get_bind_this_wrap_helper( R (ClassT::*f)() ))()
{
    struct  Wrap
    {
        static R run()
        {
            void * self=NULL;
            R (* func)(void *)=NULL;
            asm("movq %%r10, %0" :"=r"(self)::);
            asm("movq %%r11, %0" :"=r"(func)::);
            return func(self);
        }
    };
    return &Wrap::run;
}

#define CONET_MAX_BIND_THIS_PARAM_NUM 20

#define CONET_GET_BIND_THIS_WRAP_HELPER_IMPL(z, n, t) \
template<typename ClassT, typename R,  BOOST_PP_ENUM_PARAMS(n, typename arg_type_) > \
inline \
R (*get_bind_this_wrap_helper(R (ClassT::*f)(BOOST_PP_ENUM_PARAMS(n, arg_type_)))) \
        (BOOST_PP_ENUM_PARAMS(n, arg_type_)) \
{ \
    struct  Wrap \
    { \
        static R run( BOOST_PP_REPEAT(n, CONET_ARG_DEF, d) ) \
        { \
            void * self=NULL; \
            R (* func)(void *, BOOST_PP_ENUM_PARAMS(n, arg_type_)) = NULL; \
            asm("movq %%r10, %0" :"=r"(self)::); \
            asm("movq %%r11, %0" :"=r"(func)::); \
            return func(self, BOOST_PP_ENUM_PARAMS(n, arg)); \
        } \
    }; \
    return &Wrap::run; \
}


BOOST_PP_REPEAT_FROM_TO(1, BOOST_PP_INC(CONET_MAX_BIND_THIS_PARAM_NUM), CONET_GET_BIND_THIS_WRAP_HELPER_IMPL, t)


#define BindThis(obj, func) \
    ({ \
        conet::BindThisData *d = conet::get_bind_this_data();  \
        d->jump_func = (uint64_t)(conet::get_bind_this_wrap_helper(&func)); \
        d->self = (uint64_t )(&obj); \
        typeof(&func) pfn = &func; \
        memcpy(&d->mem_func, (void const *)(&pfn), sizeof(uint64_t)); \
        (typeof(conet::get_bind_this_wrap_helper(&func))) (&(d->code)); \
     })

}

#endif /* end of include guard */

