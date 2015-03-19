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
 *         Author:  piboye
 *   Organization:
 *
 * =====================================================================================
 */
#ifndef __GC_H_INC__
#define __GC_H_INC__

#include "base/list.h"

namespace conet
{

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
        (obj+i)->~T();
        //~T(obj+i);
    }
}

template <typename T>
T*  gc_alloc(int num, gc_mgr_t *mgr, bool add_to_tail=true)
{
    if (num <=0) return NULL;

    size_t len = sizeof(gc_block_t) + num * sizeof(T);
    gc_block_t * p = (gc_block_t *) malloc(len);
    p-> destructor = NULL;
    p-> num  = num ;

    INIT_LIST_HEAD(&p->link);
    if (add_to_tail) {
        list_add_tail(&p->link, &mgr->alloc_list);
    }
    else {
        list_add(&p->link, &mgr->alloc_list);
    }

    return (T*)(p->data);
}

#define GC_NEW(T, ...) \
({ \
    conet::gc_mgr_t *mgr = conet::get_gc_mgr(); \
    size_t len = sizeof(conet::gc_block_t) + sizeof(T); \
    conet::gc_block_t * p = (conet::gc_block_t *) malloc(len); \
    p-> destructor = &conet::destructor_proxy<T>; \
    p-> num  = 1; \
    ::new (p->data) T ##__VA_ARGS__; \
    INIT_LIST_HEAD(&p->link); \
    list_add_tail(&p->link, &mgr->alloc_list); \
    (T*)(p->data); \
})

template <typename T>
T*  gc_new(int num, gc_mgr_t *mgr, bool add_to_tail = true)
{
    if (num <=0) return NULL;

    size_t len = sizeof(gc_block_t) + num * sizeof(T);
    gc_block_t * p = (gc_block_t *) malloc(len);
    p-> destructor = &destructor_proxy<T>;
    p-> num  = num ;
    ::new (p->data) T[num];

    INIT_LIST_HEAD(&p->link);
    if (add_to_tail) {
        list_add_tail(&p->link, &mgr->alloc_list);
    }
    else {
        list_add(&p->link, &mgr->alloc_list);
    }

    return (T*)(p->data);
}


void init_gc_mgr(gc_mgr_t *mgr);
void gc_free_all(gc_mgr_t *mgr);
void gc_free(void *p);

gc_mgr_t *get_gc_mgr();

class ScopeGC
{
public:
    gc_block_t *m_block;
    gc_mgr_t * m_gc_mgr;
    explicit
    ScopeGC(gc_mgr_t *mgr)
    {

        m_block = (gc_block_t *) malloc(sizeof(gc_block_t));
        m_block-> destructor = NULL;
        m_block-> num  = 1;

        INIT_LIST_HEAD(&m_block->link);
        list_add_tail(&m_block->link, &mgr->alloc_list);
        m_gc_mgr = mgr;
    }

    ~ScopeGC()
    {
        gc_mgr_t mgr;
        init_gc_mgr(&mgr);

        list_cut_position(&mgr.alloc_list,
                          &m_gc_mgr->alloc_list,
                          &m_block->link
                         );

        gc_free_all(&mgr);
    }
};

// like new ClassA[num];
#define GC_NEW_ARRAY(type, num) conet::gc_new<type>(num, conet::get_gc_mgr())

// like (TypeA *) malloc(sizeof(TypeA))
#define GC_ALLOC(type) conet::gc_alloc<type>(1, conet::get_gc_mgr())

// like (TypeA *) malloc(num * sizeof(TypeA))
#define GC_ALLOC_ARRAY(type, num) conet::gc_alloc<type>(num, conet::get_gc_mgr())

#define MAKE_GC_LEVEL() \
    conet::ScopeGC gc_scope_level_ ## __LINE__(conet::get_gc_mgr())

}

#endif /* end of include guard */
