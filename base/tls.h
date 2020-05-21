/*
 * =====================================================================================
 *
 *       Filename:  tls.h
 *
 *    Description:
 *
 *        Version:  1.0
 *        Created:  04/23/2014 05:32:38 PM
 *       Revision:  none
 *       Compiler:  gcc
 *
 *         Author:  piboyeliu
 *   Organization:
 *
 * =====================================================================================
 */

//Tls.h
#ifndef __TLS_H_INC__
#define __TLS_H_INC__
#include <pthread.h>
#include "gcc_builtin_help.h"

namespace conet
{

int tls_onexit_add(void *arg, void (*free_fn)(void *));

template<typename T>
inline
void tls_destructor_help(void * arg)
{
    T * obj = (T*)(arg);
    delete obj;
}

/*
template<typename T>
inline
T * tls_get(pthread_key_t  key)
{
    T * val= (T *) pthread_getspecific(key);
    if (NULL == val) {
        val = new T();
        pthread_setspecific(key, val);
        conet::tls_onexit_add(val, conet::tls_destructor_help<T>);
    }
    return val;
}


template<typename T>
inline
T * tls_get(T * & val)
{
    T * v = val;
    if (NULL == v) {
        v = new T();
        conet::tls_onexit_add(v, conet::tls_destructor_help<T>);
        val = v;
    }
    return v;
}

#define tls_get_ref(a)  (*tls_get(a))

#define DEF_TLS_GET(val, create, fini) \
inline \
typeof(val) tls_get(typeof(val) &val) \
{  \
    typeof(val) v = val; \
    if (NULL == v) {  \
        v = create; \
        conet::tls_onexit_add(v, (void (*)(void *))&fini); \
        val = v; \
    }  \
    return v; \
} \

*/


#define CONET_DEF_TLS_VAR_HELP(var_name, init_exp, fini_exp) \
inline \
typeof(var_name) conet_tls_##var_name##_create() \
{ \
  typeof(var_name)  self = init_exp; \
  return self; \
} \
inline \
void conet_tls_##var_name##_fini(void *arg) \
{ \
  typeof(var_name)  self = (typeof(var_name))(arg); \
  { \
    fini_exp; \
  } \
  var_name = NULL; \
  return; \
} \
inline \
typeof(var_name) conet_tls_get_##var_name() \
{ \
   typeof(var_name) v = var_name;  \
   if (unlikely(NULL == v)) { \
       v = conet_tls_##var_name##_create(); \
       if (var_name ==  NULL) { \
           var_name =v; \
           conet::tls_onexit_add(v, &conet_tls_##var_name##_fini); \
       } else { \
           conet_tls_##var_name##_fini(v); \
       } \
   } \
   return var_name; \
} \
static int __attribute__((__unused__)) conet_tls_##var_name##_unused_end = 0 \


#define CONET_DEF_TLS_VAR(var_type, var_name, init_exp, fini_exp) \
__thread var_type * var_name=NULL; \
CONET_DEF_TLS_VAR_HELP(var_name, init_exp, fini_exp)


#define CONET_DEF_TLS_VAR_HELP_DEF(var_name) \
    CONET_DEF_TLS_VAR_HELP(var_name, ({ new typeof(*var_name); }), ({ delete self;}) ) \

#define CONET_DEF_TLS_GET(var_name, create_exp, fini_func)  \
inline \
typeof(var_name) conet_tls_get_##var_name() \
{ \
   typeof(var_name) v = var_name;  \
   if (unlikely(NULL == v)) { \
       v = (create_exp); \
       if (var_name ==  NULL) { \
           var_name =v; \
           conet::tls_onexit_add(v, (void (*)(void *))&fini_func);\
       } else { \
           fini_func(v); \
       } \
   } \
   return var_name; \
} \
static int __attribute__((__unused__)) conet_tls_##var_name##_unused_end = 0 \

#define TLS_GET(name) (conet_tls_get_##name ())

}

#endif
