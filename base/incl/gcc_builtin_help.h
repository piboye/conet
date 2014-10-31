/*
 * =====================================================================================
 *
 *       Filename:  gcc_builtin_help.h
 *
 *    Description:  
 *
 *        Version:  1.0
 *        Created:  2014年11月01日 01时46分27秒
 *       Revision:  none
 *       Compiler:  gcc
 *
 *         Author:  piboye
 *   Organization:  
 *
 * =====================================================================================
 */

#ifndef __CONET_GCC_BUILTIN_HELP_H__
#define __CONET_GCC_BUILTIN_HELP_H__


#define likely(x) __builtin_expect(!!(x), 1)
#define unlikely(x) __builtin_expect(!!(x), 0)


#endif /* end of include guard */
