/*
 * =====================================================================================
 *
 *       Filename:  tls.cpp
 *
 *    Description:  
 *
 *        Version:  1.0
 *        Created:  04/23/2014 05:33:42 PM
 *       Revision:  none
 *       Compiler:  gcc
 *
 *         Author:  piboyeliu
 *   Organization:  
 *
 * =====================================================================================
 */
#include <stdlib.h>
#include <stdio.h>
#include <sys/syscall.h>
#include "tls.h"
#include "list.h"

#define LOG(...)
 
#define gettid() syscall(__NR_gettid)
#define TLS_OUT_OF_INDEXES          0xffffffff

struct pthread_atexit_t
{
    void   (*free_fn)(void *);
    void   *arg;
    list_head link_to;
};
 
 
static 
pthread_key_t   g_pthread_atexit_key = TLS_OUT_OF_INDEXES;

static 
pthread_once_t  g_pthread_atexit_control_once = PTHREAD_ONCE_INIT;
 
static 
void pthread_atexit_done(void *arg)
{
    list_head *list = (list_head *) arg;
    if (NULL == list) return;
    pthread_atexit_t *id_ptr=NULL;

    list_head *it=NULL, *next=NULL;
    list_for_each_safe(it, next, list)
    {
        id_ptr = container_of(it, pthread_atexit_t, link_to);
        if (id_ptr->free_fn)
            id_ptr->free_fn(id_ptr->arg);
        list_del(it);
        free(id_ptr);
    }
    free(list); 
}
 
static 
void pthread_atexit_init(void)
{
    pthread_key_create(&g_pthread_atexit_key, pthread_atexit_done);
}

 
int tls_onexit_add(void *arg, void (*free_fn)(void *))
{
    pthread_once(&g_pthread_atexit_control_once, pthread_atexit_init);
    if (g_pthread_atexit_key == (pthread_key_t) TLS_OUT_OF_INDEXES)
    {
        LOG("_pthread_atexit_key(%d) invalid\n", g_pthread_atexit_key);
        return (-1);
    }

    list_head * list = (list_head *)pthread_getspecific(g_pthread_atexit_key); 
    if (NULL == list) {
        list = (list_head *)malloc(sizeof(list_head));
        INIT_LIST_HEAD(list);
        pthread_setspecific(g_pthread_atexit_key, list);
    }

    pthread_atexit_t *item = (pthread_atexit_t *)malloc(sizeof(pthread_atexit_t));
    INIT_LIST_HEAD(&item->link_to); 
    item->free_fn = free_fn;
    item->arg = arg;
    list_add(&item->link_to, list);
    return 0;
}
