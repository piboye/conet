/*
 * =====================================================================================
 *
 *       Filename:  gc.h
 *
 *    Description:  
 *
 *        Version:  1.0
 *        Created:  2014年05月13日 16时42分43秒
 *       Revision:  none
 *       Compiler:  gcc
 *
 *         Author:  piboyeliu
 *   Organization:  
 *
 * =====================================================================================
 */
#ifndef __GC_H_INC__
#define __GC_H_INC__

#include "list.h"

struct gc_mgr_t
{
    list_head alloc_list;    
};

struct gc_block_t
{
    list_head link;
    void (*destructor)(void *, int num);
    int64_t num;
    char data[0];
};

template <typename T>
void destructor_proxy(void * arg, int num) 
{
   T * obj = (T*)(arg);
   for (int i=0; i<num; ++i) {
      ~T(obj+i);
   }
}

template <typename T>
T*  gc_alloc(int num, gc_mgr_t *mgr) 
{
    if (num <=0) return NULL;

    size_t len = sizeof(gc_block_t) + num * sizeof(T);
    gc_block_t * p = (gc_block_t *) malloc(len);
    p-> destructor = NULL; 
    p-> num  = num ;

    INIT_LIST_HEAD(&p->link);
    list_add_tail(&p->link, &mgr->alloc_list);
     
    return (T*)(p->data);
}

template <typename T>
T*  gc_new(int num, gc_mgr_t *mgr) 
{
    if (num <=0) return NULL;

    size_t len = sizeof(gc_block_t) + num * sizeof(T);
    gc_block_t * p = (gc_block_t *) malloc(len);
    p-> destructor = &destructor_proxy<T>; 
    p-> num  = num ;
    ::new (p->data) T[num];

    list_add_tail(&p->link, &mgr->alloc_list);
     
    return (T*)(p->data);
}

void init_gc_mgr(gc_mgr_t *mgr);
void gc_free_all(gc_mgr_t *mgr);

void gc_free(void *p);

#endif /* end of include guard */
