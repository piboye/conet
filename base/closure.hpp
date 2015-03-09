/*
 * =====================================================================================
 *
 *       Filename:  closure.hpp
 *
 *    Description:  
 *
 *        Version:  1.0
 *        Created:  03/09/2015 10:37:36 AM
 *       Revision:  none
 *       Compiler:  gcc
 *
 *         Author:  piboye
 *   Organization:  
 *
 * =====================================================================================
 */

#ifndef __CONET_CLOSURE_H__
#define __CONET_CLOSURE_H__

#define BOOST_PP_VARIADICS 1

#include "boost/preprocessor.hpp"

namespace conet
{

struct closure_base_t
{

    virtual ~closure_base_t();
};


#define CONET_CLOSURE_ARG_DEF(z, n, t) \
            BOOST_PP_COMMA_IF(BOOST_PP_DEC(n)) \
                BOOST_PP_CAT(arg_type_,n) BOOST_PP_CAT(arg, n)

#define CONET_DEFINE_CLOSURE_IMPL(z, n, t)  CONET_DEFINE_CLOSURE_IMPL2(BOOST_PP_INCR(n)) \

#define CONET_DEFINE_CLOSURE_IMPL2(n)  \
template  \
    <  \
BOOST_PP_ENUM_PARAM(n, typename arg_type_)  > \
struct closure_t : public closure_base_t\
{  \
    typedef arg_type_0 return_type; \
    virtual arg_type_0 Run( \
            BOOST_PB_REPEAT_FROM_TO(1, n, CONET_CLOSURE_ARG_DEF, d) \
            );\
};

BOOST_PP_REPEAT(20, CONET_DEFINE_CLOSURE_IMPL, t)

#define GET_CLOSURE_TYPE_BY_FUNC(z, n, t) GET_CLOSURE_TYPE_BY_FUNC2(BOOST_PP_INCR(n))  \

#define GET_CLOSURE_TYPE_BY_FUNC2(n)  \
template  < \
    BOOST_PP_ENUM_PARAM(n, typename arg_type_)   \
    > \
\
closure_t<BOOST_PP_ENUM_PARAM(n, typename arg_type_)> \
 * get_closure_type_by_func(\
            arg_type_0 (*fn)(BOOST_PB_REPEAT_FROM_TO(1, n, CONET_CLOSURE_ARG_DEF, d) \
        ) \
 { \
    return new closure_t<\
    BOOST_PP_ENUM_PARAM(n, arg_type_) >();\
 } \


BOOST_PP_REPEAT(20, GET_CLOSURE_TYPE_BY_FUNC, t)

#define ConetNewClosure(return_type, param, a_copy, body) \
({ \
   typedef(*(&conet::get_closure_type_by_func(return_type (*0)param))) closuser_type;
\})


}

#endif /* end of include guard */ 
