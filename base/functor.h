/*
 * =====================================================================================
 *
 *       Filename:  functor.h
 *
 *    Description:  
 *
 *        Version:  1.0
 *        Created:  2014年12月18日 05时38分35秒
 *       Revision:  none
 *       Compiler:  gcc
 *
 *         Author:  piboye
 *   Organization:
 *
 * =====================================================================================
 */

#ifndef __CONET_FUNCTOR_H__
#define __CONET_FUNCTOR_H__

#define BOOST_PP_VARIADICS 1
#include "boost/preprocessor.hpp"
#include "closure.h"

#define CONET_REF_DECL_TYPEOF(r, data,  a) typedef typeof(a) BOOST_PP_CAT(typeof_, a) ;
#define CONET_REF_IMPL_REF_TYPE(r, data, a) BOOST_PP_CAT(typeof_,a) &a;
#define CONET_COPY_IMPL_REF_TYPE(r, data, a) BOOST_PP_CAT(typeof_,a) a;
#define CONET_REF_PARAM_DEF(r1, data, i, a) BOOST_PP_COMMA_IF(i) BOOST_PP_CAT(typeof_,a) & BOOST_PP_CAT(a,r)
#define CONET_COPY_PARAM_DEF(r1, data, i, a) BOOST_PP_COMMA_IF(i) BOOST_PP_CAT(typeof_,a) const & BOOST_PP_CAT(a,r)
#define CONET_REF_PARAM_INIT(r1, data, i, a) BOOST_PP_COMMA_IF(i) a(BOOST_PP_CAT(a,r))


#define MAKE_REF_IMPL(name, list)\
BOOST_PP_SEQ_FOR_EACH(CONET_REF_DECL_TYPEOF, data, list) \
struct __ref_type_##name\
{\
    BOOST_PP_SEQ_FOR_EACH(CONET_REF_IMPL_REF_TYPE, data, list) \
	__ref_type_##name ( \
        BOOST_PP_SEQ_FOR_EACH_I(CONET_REF_PARAM_DEF, data, list) \
    ): \
        BOOST_PP_SEQ_FOR_EACH_I(CONET_REF_PARAM_INIT, data, list) \
	{}\
}

#define MAKE_REF(name, vars) MAKE_REF_IMPL(name, BOOST_PP_VARIADIC_TO_SEQ vars) name vars

#define MAKE_COPY(name , vars ) MAKE_COPY_IMPL(name, BOOST_PP_VARIADIC_TO_SEQ vars) name vars

#define MAKE_COPY_IMPL(name, list)\
BOOST_PP_SEQ_FOR_EACH(CONET_REF_DECL_TYPEOF, data, list) \
class __copy_type_##name\
{\
public:\
    BOOST_PP_SEQ_FOR_EACH(CONET_COPY_IMPL_REF_TYPE, data, list) \
	__copy_type_##name( \
        BOOST_PP_SEQ_FOR_EACH_I(CONET_REF_PARAM_DEF, data, list) \
    ): \
        BOOST_PP_SEQ_FOR_EACH_I(CONET_REF_PARAM_INIT, data, list) \
	{}\
}


// 创建一个函数 , 返回是Closure
#define NewFunc(return_type, param, ...) \
    BOOST_PP_IF(BOOST_PP_GREATER(BOOST_PP_VARIADIC_SIZE(__VA_ARGS__) , 1), \
            NewFuncWithCopy, NewFuncRaw) \
            (return_type, param, __VA_ARGS__)


// 创建一个函数， 不拷贝局部变量
#define NewFuncRaw(return_type, param, body) \
    ({ \
     typedef typeof(*NewClosure((return_type (*) param)(NULL))) cl_type; \
     struct  conet_functor_ :  \
        public cl_type \
     {  \
       bool IsSelfDelete() const { return false;} \
       return_type Run param \
              body \
     };  new conet_functor_();})

// 创建一个函数， 并拷贝局部变量
#define NewFuncWithCopy(return_type, param, a_copy, body) \
    ({ \
     typedef typeof(*NewClosure((return_type (*) param)(NULL))) cl_type; \
     BOOST_PP_SEQ_FOR_EACH(CONET_REF_DECL_TYPEOF, data, BOOST_PP_VARIADIC_TO_SEQ a_copy) \
     struct  conet_functor_ :  \
        public cl_type \
     {  \
        BOOST_PP_SEQ_FOR_EACH(CONET_COPY_IMPL_REF_TYPE, data, BOOST_PP_VARIADIC_TO_SEQ a_copy) \
        conet_functor_( \
            BOOST_PP_SEQ_FOR_EACH_I(CONET_COPY_PARAM_DEF, data, BOOST_PP_VARIADIC_TO_SEQ a_copy) \
            ): \
            BOOST_PP_SEQ_FOR_EACH_I(CONET_REF_PARAM_INIT, data, BOOST_PP_VARIADIC_TO_SEQ a_copy) \
        { \
        } \
       bool IsSelfDelete() const { return false;} \
       return_type Run param  \
              body \
      };  new conet_functor_ a_copy;})


#endif /* end of include guard */
