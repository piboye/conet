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
    virtual ~closure_base_t() {}
};

// 定义 closure 的最大参数个数
#ifndef CONET_MAX_CLOSURE_PARAM_NUM
# define CONET_MAX_CLOSURE_PARAM_NUM 20
#endif

#define CONET_CLOSURE_ARG_DEF(z, n, t) \
            BOOST_PP_COMMA_IF(BOOST_PP_DEC(n)) \
                BOOST_PP_CAT(arg_type_,n) BOOST_PP_CAT(arg, n)

#define CONET_CLOSURE_TPL_TYPE_DEF(z, n, t) \
            BOOST_PP_COMMA_IF(n) \
                typename BOOST_PP_CAT(arg_type_,n) = void


template < BOOST_PP_REPEAT(BOOST_PP_INC(CONET_MAX_CLOSURE_PARAM_NUM), CONET_CLOSURE_TPL_TYPE_DEF, t)>
struct closure_t : public closure_base_t
{
    typedef arg_type_0 return_type;
    virtual arg_type_0 Run(
                BOOST_PP_REPEAT_FROM_TO(1, CONET_MAX_CLOSURE_PARAM_NUM, CONET_CLOSURE_ARG_DEF, d)
            ) = 0;
};

#define CONET_DEFINE_CLOSURE_IMPL(z, n, t)  CONET_DEFINE_CLOSURE_IMPL2(BOOST_PP_INC(n)) \

#define CONET_DEFINE_CLOSURE_IMPL2(n)  \
template  \
    <  BOOST_PP_ENUM_PARAMS(n, typename arg_type_)  > \
struct closure_t \
    < BOOST_PP_ENUM_PARAMS(n, arg_type_)  > \
: public closure_base_t\
{  \
    typedef arg_type_0 return_type; \
    virtual arg_type_0 Run( \
            BOOST_PP_REPEAT_FROM_TO(1, n, CONET_CLOSURE_ARG_DEF, d) \
            )=0;\
};

BOOST_PP_REPEAT(CONET_MAX_CLOSURE_PARAM_NUM, CONET_DEFINE_CLOSURE_IMPL, t)

#define GET_CLOSURE_TYPE_BY_FUNC(z, n, t) GET_CLOSURE_TYPE_BY_FUNC2(BOOST_PP_INC(n))  \

#define GET_CLOSURE_TYPE_BY_FUNC2(n)  \
template  < \
    BOOST_PP_ENUM_PARAMS(n, typename arg_type_)   \
    > \
\
closure_t<BOOST_PP_ENUM_PARAMS(n, arg_type_)> \
 * get_closure_type_by_func(\
            arg_type_0 (*fn)(BOOST_PP_REPEAT_FROM_TO(1, n, CONET_CLOSURE_ARG_DEF, d)) \
        ) \
 { \
    return new closure_t<BOOST_PP_ENUM_PARAMS(n, arg_type_) >();\
 } \


BOOST_PP_REPEAT(CONET_MAX_CLOSURE_PARAM_NUM, GET_CLOSURE_TYPE_BY_FUNC, t)

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


// 创建Closure
#define NewClosure(return_type, param, ...) \
    BOOST_PP_IF(BOOST_PP_GREATER(BOOST_PP_VARIADIC_SIZE(__VA_ARGS__) , 1), \
            NewClosureWithCopy, NewClosureRaw) \
            (return_type, param, __VA_ARGS__)


// 创建一个Closure， 不拷贝局部变量
#define NewClosureRaw(return_type, param, body) \
    ({ \
     typedef typeof(*conet::get_closure_type_by_func((return_type (*) param)(NULL))) cl_type; \
     struct  conet_functor_ :  \
        public cl_type \
     {  \
       bool IsSelfDelete() const { return false;} \
       return_type Run param \
              body \
     };  new conet_functor_();})

// 创建一个Closure， 并拷贝局部变量
#define NewClosureWithCopy(return_type, param, a_copy, body) \
    ({ \
     typedef typeof(*get_closure_type_by_func((return_type (*) param)(NULL))) cl_type; \
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
       return_type Run param  \
              body \
      };  new conet_functor_ a_copy;})

// 创建一个Closure， 并引用局部变量
#define NewClosureWithRef(return_type, param, a_ref, body) \
    ({ \
     typedef typeof(*get_closure_type_by_func((return_type (*) param)(NULL))) cl_type; \
     BOOST_PP_SEQ_FOR_EACH(CONET_REF_DECL_TYPEOF, data, BOOST_PP_VARIADIC_TO_SEQ a_copy) \
     struct  conet_functor_ :  \
        public cl_type \
     {  \
        BOOST_PP_SEQ_FOR_EACH(CONET_REF_IMPL_REF_TYPE, data, BOOST_PP_VARIADIC_TO_SEQ a_copy) \
        conet_functor_( \
            BOOST_PP_SEQ_FOR_EACH_I(CONET_REF_PARAM_DEF, data, BOOST_PP_VARIADIC_TO_SEQ a_copy) \
            ): \
            BOOST_PP_SEQ_FOR_EACH_I(CONET_REF_PARAM_INIT, data, BOOST_PP_VARIADIC_TO_SEQ a_copy) \
        { \
        } \
       return_type Run param  \
              body \
      };  new conet_functor_ a_copy;})

}

#endif /* end of include guard */ 
