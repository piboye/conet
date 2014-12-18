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

namespace conet
{


#define new_func_def(class_T, return_type, func_name, param) \
    ({ struct  conet_functor_ :public class_T { return_type func_name param { \

     
#define new_func_end \
     } };  new conet_functor_(); })

#define CONET_REMOVE_BRA_(...) __VA_ARGS__

}

#endif /* end of include guard */
