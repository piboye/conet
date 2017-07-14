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
 *         Author:  piboye
 *   Organization:
 *
 * =====================================================================================
 */
#include <stdlib.h>
#include <stdio.h>
#include <unistd.h>
#include <sys/syscall.h>
#include "tls.h"
#include "list.h"
#include "../plog.h"

#define gettid() syscall(__NR_gettid)
#define TLS_OUT_OF_INDEXES          0xffffffff

namespace conet
{
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

    list_head l;
    INIT_LIST_HEAD(&l);
    list_add(&l, list);
    list_del_init(list);

    list_for_each_safe(it, next, &l)
    {
        list_del(it);
        id_ptr = container_of(it, pthread_atexit_t, link_to);
        if (id_ptr->free_fn)
            id_ptr->free_fn(id_ptr->arg);
        delete id_ptr;
    }
    delete list;
}


static void main_thread_atexit_done()
{
    return pthread_atexit_done(pthread_getspecific(g_pthread_atexit_key));
}

static
void pthread_atexit_init(void)
{
    pthread_key_create(&g_pthread_atexit_key, pthread_atexit_done);

    //主线程 退出释放资源
    atexit(&main_thread_atexit_done);
}


int tls_onexit_add(void *arg, void (*free_fn)(void *))
{
    pthread_once(&g_pthread_atexit_control_once, pthread_atexit_init);
    if (g_pthread_atexit_key == (pthread_key_t) TLS_OUT_OF_INDEXES)
    {
        PLOG_ERROR("pthread_atexit_key(", g_pthread_atexit_key, ") invalid");
        return (-1);
    }

    list_head * list = (list_head *)pthread_getspecific(g_pthread_atexit_key);
    if (NULL == list) {
        list = (list_head *)new (list_head);
        INIT_LIST_HEAD(list);
        pthread_setspecific(g_pthread_atexit_key, list);
    }

    pthread_atexit_t *item = (pthread_atexit_t *)(new(pthread_atexit_t));
    INIT_LIST_HEAD(&item->link_to);
    item->free_fn = free_fn;
    item->arg = arg;
    list_add(&item->link_to, list);
    return 0;
}
}
