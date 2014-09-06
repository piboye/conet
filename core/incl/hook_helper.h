/*
 * =====================================================================================
 *
 *       Filename:  hook_helper.h
 *
 *    Description:
 *
 *        Version:  1.0
 *        Created:  2014年05月21日 03时44分00秒
 *       Revision:  none
 *       Compiler:  gcc
 *
 *         Author:  piboyeliu
 *   Organization:
 *
 * =====================================================================================
 */
#ifndef __HOOK_HELLPER_H__
#define __HOOK_HELLPER_H__

#include <dlfcn.h>

#define SYS_FUNC(name) g_sys_##name##_func
#define _(name) SYS_FUNC(name)

#define HOOK_SYS_FUNC_DEF(ret_type, name, proto) \
    typedef ret_type (* name##_pfn_t) proto; \
    name##_pfn_t _(name) = (name##_pfn_t) dlsym(RTLD_NEXT, #name); \
    extern "C"  ret_type name proto __attribute__ ((visibility ("default")));  \
    ret_type name proto \
 
#define HOOK_CPP_FUNC_DEF(ret_type, name, proto) \
    typedef ret_type (* name##_pfn_t) proto; \
    name##_pfn_t _(name) = (name##_pfn_t) dlsym(RTLD_NEXT, #name); \
    ret_type name proto \

#define HOOK_SYS_FUNC(name) if( !_(name)) { _(name) = (name##_pfn_t)dlsym(RTLD_NEXT,#name); }

#define HOOK_DECLARE(ret_type, name, proto) \
    typedef ret_type (* name##_pfn_t) proto; \
    extern "C" name##_pfn_t _(name)


#endif /* end of include guard */
