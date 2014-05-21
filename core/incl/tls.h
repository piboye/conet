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

int tls_onexit_add(void *arg, void (*free_fn)(void *));

template<typename T>
inline 
void tls_destructor_fun(void * arg) 
{
    T * obj = (T*)(arg);
    delete obj;
}

template<typename T>
inline
T * tls_get(pthread_key_t  key) 
{
    T * val= (T *) pthread_getspecific(key);
    if (NULL == val) {
        val = new T();
        tls_onexit_add(val, tls_destructor_fun<T>);
    }
    return val;
}

    
template<typename T>
inline
T * tls_get(T * & val)
{
    if (NULL == val) {
        val = new T();
        tls_onexit_add(val, tls_destructor_fun<T>);
    }
    return val;
}

#define tls_get_ref(a)  (*tls_get(a))

#define DEF_TLS_GET(val, create, fini) \
inline \
typeof(val) tls_get(typeof(val) &val) \
{  \
    if (NULL == val) {  \
        val = create; \
        tls_onexit_add(val, (void (*)(void *))&fini); \
    }  \
    return val; \
} \



#endif
