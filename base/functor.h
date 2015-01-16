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
#include "closure.h"

namespace conet
{

//1.base 
//-- 1.1 comac_argc

#define comac_get_args_cnt( ... ) comac_arg_n( __VA_ARGS__ )
#define comac_arg_n(_0, _1,_2,_3,_4,_5,_6,_7,N,...) N
#define comac_args_seqs() 7,6,5,4,3,2,1,0
#define comac_join_1( x,y ) x##y

#define comac_argc( ... ) comac_get_args_cnt(__VA_ARGS__, comac_args_seqs() )
#define comac_join( x,y) comac_join_1( x,y )

//-- 1.2 repeat
#define repeat_0( fun,a, ... ) 
#define repeat_1( fun,a, ... ) fun( 1,a,__VA_ARGS__ ) repeat_0( fun,__VA_ARGS__ )
#define repeat_2( fun,a, ... ) fun( 2,a,__VA_ARGS__ ) repeat_1( fun,__VA_ARGS__ )
#define repeat_3( fun,a, ... ) fun( 3,a,__VA_ARGS__ ) repeat_2( fun,__VA_ARGS__ )
#define repeat_4( fun,a, ... ) fun( 4,a,__VA_ARGS__ ) repeat_3( fun,__VA_ARGS__ )
#define repeat_5( fun,a, ... ) fun( 5,a,__VA_ARGS__ ) repeat_4( fun,__VA_ARGS__ )
#define repeat_6( fun,a, ... ) fun( 6,a,__VA_ARGS__ ) repeat_5( fun,__VA_ARGS__ )

#define repeat( n,fun,... ) comac_join ( repeat_,n )( fun,__VA_ARGS__)

//2.implement
#define decl_typeof( i,a,... ) typedef typeof( a ) typeof_##a;
#define impl_typeof( i,a,... ) typeof_##a & a;
#define impl_typeof_cpy( i,a,... ) typeof_##a a;
#define con_param_typeof( i,a,... ) typeof_##a & a##r,
#define cpy_param_typeof( i,a,... ) typeof_##a a##r,
#define param_init_typeof( i,a,... ) a(a##r),


//2.1 reference

#define co_ref( name,... )\
repeat( comac_argc(__VA_ARGS__) ,decl_typeof,__VA_ARGS__ )\
class type_##name\
{\
public:\
	repeat( comac_argc(__VA_ARGS__) ,impl_typeof,__VA_ARGS__ )\
	int _member_cnt;\
	type_##name( \
		repeat( comac_argc(__VA_ARGS__),con_param_typeof,__VA_ARGS__ ) ... ): \
		repeat( comac_argc(__VA_ARGS__),param_init_typeof,__VA_ARGS__ ) _member_cnt(comac_argc(__VA_ARGS__)) \
	{}\
} name( __VA_ARGS__ ) ;

#define CONET_REMOVE_BRA_2(...) __VA_ARGS__

#define CONET_REMOVE_BRA_(a) CONET_REMOVE_BRA_2 a

#define NewFunc(return_type, param, a_copy, body) \
    ({ \
     typedef typeof(*NewClosure((return_type (*) param)(NULL))) cl_type; \
      repeat(comac_argc(CONET_REMOVE_BRA_(a_copy)),  \
          decl_typeof, \
          CONET_REMOVE_BRA_(a_copy)) \
      \
     struct  conet_functor_ :  \
        public cl_type \
     {  \
      repeat(comac_argc(CONET_REMOVE_BRA_(a_copy)),  \
          impl_typeof_cpy, \
          CONET_REMOVE_BRA_(a_copy))  \
      \
       int __cpy_param_num; \
         virtual bool IsSelfDelete() const { return false;} \
         conet_functor_( \
            repeat(comac_argc(CONET_REMOVE_BRA_(a_copy)), \
                cpy_param_typeof, \
                CONET_REMOVE_BRA_(a_copy))  ...) :\
                  repeat(comac_argc(CONET_REMOVE_BRA_(a_copy)), param_init_typeof, CONET_REMOVE_BRA_(a_copy)) \
                  __cpy_param_num(comac_argc(CONET_REMOVE_BRA_(a_copy)))  \
             {}  \
         virtual ~conet_functor_() { } \
      virtual return_type Run param { \
              body \
     } };  new conet_functor_ a_copy;})


#define DEFER(param, op)  \
struct __Defer##__LINE__ \
{ \
   repeat( comac_argc param ,impl_typeof, CONET_REMOVE_BRA_(param) )\
	int _member_cnt;\
	__Defer##__LINE__( \
		repeat( comac_argc param,con_param_typeof, CONET_REMOVE_BRA_(param) ) ... ): \
		repeat( comac_argc ,param_init_typeof, CONET_REMOVE_BRA(param) ) _member_cnt(comac_argc param) \
	{}\
    ~__Defer##__LINE__() \
    op  \
} __defer_##__LINE__ param \

}

#endif /* end of include guard */
