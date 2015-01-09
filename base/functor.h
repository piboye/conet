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

#define CONET_REMOVE_BRA_2(...) __VA_ARGS__

#define CONET_REMOVE_BRA_(a) CONET_REMOVE_BRA_2 a

#define DEF_LOCA_DATA_TYPE(name, a_copy, a_ref) \
      repeat(comac_argc(CONET_REMOVE_BRA_(a_ref)),  \
          decl_typeof, \
          CONET_REMOVE_BRA_(a_ref))  \
      \
      repeat(comac_argc(CONET_REMOVE_BRA_(a_copy)),  \
          decl_typeof, \
          CONET_REMOVE_BRA_(a_copy)) \
      \
    struct name \
    {\
      repeat(comac_argc(CONET_REMOVE_BRA_(a_ref)),  \
          impl_typeof, \
          CONET_REMOVE_BRA_(a_ref))  \
      \
       int _ref_cnt;  \
      repeat(comac_argc(CONET_REMOVE_BRA_(a_copy)),  \
          impl_typeof_cpy, \
          CONET_REMOVE_BRA_(a_copy))  \
      \
       int _cpy_cnt;  \
        name( \
            repeat(\
                comac_argc(CONET_REMOVE_BRA_(a_ref)),\
                con_param_typeof, \
                CONET_REMOVE_BRA_(a_ref) \
            )   \
            repeat(comac_argc(CONET_REMOVE_BRA_(a_copy)), \
                cpy_param_typeof, \
                CONET_REMOVE_BRA_(a_copy)  \
        ) ...): \
                repeat(comac_argc(CONET_REMOVE_BRA_(a_ref)), param_init_typeof, CONET_REMOVE_BRA_(a_ref))  \
                _ref_cnt(comac_argc(CONET_REMOVE_BRA_(a_ref))), \
                repeat(comac_argc(CONET_REMOVE_BRA_(a_copy)), param_init_typeof, CONET_REMOVE_BRA_(a_copy))  \
                _cpy_cnt(comac_argc(CONET_REMOVE_BRA_(a_copy))) \
        {} \
     }

#define TAKE1_0(_0, N1, N2, ...) N1
#define TAKE2_0(_0, N1, N2, ...) N2

#define TAKE1(...) TAKE1_0(_, ##__VA_ARGS__ , , )
#define TAKE2(...) TAKE2_0(_, ##__VA_ARGS__ , ,)

#define CTAKE1(...) TAKE1_0(_, ##__VA_ARGS__ , (), ())
#define CTAKE2(...) TAKE2_0(_, ##__VA_ARGS__ , (), ())

#define RECAT_WITH_DOT(a) , a
#define EXPAND(...) __VA_ARGS__
#define MERGE2(...) \
        CONET_REMOVE_BRA_(CTAKE1(__VA_ARGS__))  \
        repeat( \
        comac_argc(CONET_REMOVE_BRA_(CTAKE2(__VA_ARGS__))), RECAT_WITH_DOT, CONET_REMOVE_BRA_(TAKE2(__VA_ARGS__)) \
        )

#define NewFunc(return_type, param, ...) \
    ({ \
     typedef typeof(*NewClosure((return_type (*) param)(NULL))) cl_type; \
     DEF_LOCA_DATA_TYPE(local_data_type, TAKE1(__VA_ARGS__), TAKE2(__VA_ARGS__) ); \
     local_data_type * local_data =  new local_data_type( \
         MERGE2(__VA_ARGS__)); \
     struct  conet_functor_ :  \
        public cl_type \
     {  \
         local_data_type * self; \
         virtual bool IsSelfDelete() const { return false;} \
         conet_functor_(local_data_type *data): self(data){}; \
         virtual ~conet_functor_() { delete self;}; \
      virtual return_type Run param { \


#define EndFunc \
     } };  new conet_functor_(local_data);})


}

#endif /* end of include guard */
