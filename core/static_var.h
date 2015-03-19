/*
 * =====================================================================================
 *
 *       Filename:  static_var.h
 *
 *    Description:  
 *
 *        Version:  1.0
 *        Created:  03/19/2015 02:35:50 PM
 *       Revision:  none
 *       Compiler:  gcc
 *
 *         Author:  piboye
 *   Organization:  
 *
 * =====================================================================================
 */

#ifndef __STATIC_VAR_H__
#define __STATIC_VAR_H__

#include "gc.h"

namespace conet 
{

void * get_static_var(void *key);
void * set_static_var(void * key, void *val);

#define CO_DEF_STATIC_VAR(type, name, ...) \
    static int co_static_var_ct_ ## name = 0; \
    type * co_static_var_p_ ## name =  (type *) get_static_var(& co_static_var_ct_ ## name); \
    if (co_static_var_p_ ## name == NULL) { \
        co_static_var_p_ ## name = GC_NEW(type, ##__VA_ARGS__); \
        set_static_var(&co_static_var_ct_ ## name, co_static_var_p_ ## name); \
    } \
    type & name = * co_static_var_p_ ## name

#define CO_DEF_STATIC_PTR(type, name, init_val) \
    static int co_static_var_ct_ ## name = 0; \
    type * name =  (type *) get_static_var(& co_static_var_ct_ ## name); \
    if (name == NULL) { \
        name = init_val; \
        set_static_var(&co_static_var_ct_ ## name, name); \
    } \


}

#endif /* end of include guard */
