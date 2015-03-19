/*
 * =====================================================================================
 *
 *       Filename:  defer.h
 *
 *    Description:  
 *
 *        Version:  1.0
 *        Created:  2015年03月09日 15时54分04秒
 *       Revision:  none
 *       Compiler:  gcc
 *
 *         Author:  piboye
 *   Organization:  
 *
 * =====================================================================================
 */
#ifndef __CONET_DEFER_H__
#define __CONET_DEFER_H__
#include "macro_help.h"

#define CONET_DEFER_DECL_TYPEOF(r, data,  a) typedef typeof(a) BOOST_PP_CAT(typeof_, a) ;
#define CONET_DEFER_IMPL_REF_TYPE(r, data, a) BOOST_PP_CAT(typeof_,a) &a;

#define CONET_DEFER_PARAM_DEF(r1, data, i, a) BOOST_PP_COMMA_IF(i) BOOST_PP_CAT(typeof_,a) & BOOST_PP_CAT(a,r)
#define CONET_DEFER_PARAM_INIT(r1, data, i, a) BOOST_PP_COMMA_IF(i) a(BOOST_PP_CAT(a,r))


#define CONET_DEFER(...)  \
        BOOST_PP_IF(BOOST_PP_GREATER(BOOST_PP_VARIADIC_SIZE(__VA_ARGS__), 1), \
                CONET_DEFER_IMPL, CONET_DEFER_EMPTY_PARAM_IMPL) \
            (__VA_ARGS__)

#define CONET_DEFER_IMPL(param, op) CONET_DEFER_IMPL2(param, BOOST_PP_VARIADIC_TO_SEQ param, op)

#define CONET_DEFER_IMPL2(param, param_seq , op) \
BOOST_PP_SEQ_FOR_EACH(CONET_DEFER_DECL_TYPEOF, data, param_seq) \
struct __conet_defer_t_##__LINE__ \
{ \
    BOOST_PP_SEQ_FOR_EACH(CONET_DEFER_IMPL_REF_TYPE, data, param_seq) \
    explicit \
    __conet_defer_t_##__LINE__ \
    (\
        BOOST_PP_SEQ_FOR_EACH_I(CONET_DEFER_PARAM_DEF, data, param_seq) \
    ) :\
        BOOST_PP_SEQ_FOR_EACH_I(CONET_DEFER_PARAM_INIT, data, param_seq) \
	{}\
    \
    ~__conet_defer_t_##__LINE__()\
    op  \
}  \
__conet_defer_var_##__LINE__ param \

#define CONET_DEFER_EMPTY_PARAM_IMPL(op)  \
struct __conet_defer_t_##__LINE__  \
{ \
    ~__conet_defer_t_##__LINE__() \
    op \
} __conet_defer_var_##__LINE__



#endif /* end of include guard */

