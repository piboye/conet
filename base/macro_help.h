/*
 * =====================================================================================
 *
 *       Filename:  macro_help.h
 *
 *    Description:  
 *
 *        Version:  1.0
 *        Created:  03/19/2015 03:11:40 PM
 *       Revision:  none
 *       Compiler:  gcc
 *
 *         Author:  piboye
 *   Organization:  
 *
 * =====================================================================================
 */
#ifndef __CONET_MACRO_HELP_H__
#define __CONET_MACRO_HELP_H__

#ifndef BOOST_PP_VARIADICS 
//  gcc 必须定义这个， 不然会定义为 0, 导致 BOOST_PP_VARIADIC_TO_SEQ 用不了
# define BOOST_PP_VARIADICS 2 
#endif 

#include "boost/preprocessor.hpp"

#define CONET_ARG_DEF(z, n, t) \
            BOOST_PP_COMMA_IF(n) \
                BOOST_PP_CAT(arg_type_,n) BOOST_PP_CAT(arg, n)

#endif /* end of include guard */

