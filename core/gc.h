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


#define GC_NEW_WITH_INIT_PRE_EXPR(N) \
    size_t len = sizeof(gc_block_t) + sizeof(T); \
    gc_block_t * p = (gc_block_t *) malloc(len); \
    p-> destructor = &destructor_proxy<T>; \
    p-> num  = N \


#define GC_NEW_WITH_INIT_POST_EXPR(N) \
    INIT_LIST_HEAD(&p->link); \
    if (add_to_tail) { \
        list_add_tail(&p->link, &mgr->alloc_list); \
    } \
    else { \
        list_add(&p->link, &mgr->alloc_list); \
    } \
    return (T*)(p->data) \

template <typename T, typename T1>
T*  gc_new_with_init1(gc_mgr_t *mgr, T1 const &val1, bool add_to_tail=true)
{
    GC_NEW_WITH_INIT_PRE_EXPR(1);

    ::new (p->data) T(val1);

    GC_NEW_WITH_INIT_POST_EXPR(1);
}

template <typename T, typename T1, typename T2>
T*  gc_new_with_init2(gc_mgr_t *mgr, T1 const &val1,  T2 const &val2,  bool add_to_tail=true)
{
    GC_NEW_WITH_INIT_PRE_EXPR(1);

    ::new (p->data) T(val1, val2);

    GC_NEW_WITH_INIT_POST_EXPR(1);
}

template <typename T, typename T1, typename T2, typename T3>
T*  gc_new_with_init3(gc_mgr_t *mgr, T1 const &val1, T2 const &val2, T3 const &val3,  bool add_to_tail=true)
{
    GC_NEW_WITH_INIT_PRE_EXPR(1);

    ::new (p->data) T(val1, val2, val3);

    GC_NEW_WITH_INIT_POST_EXPR(1);
}

template <typename T, typename T1, typename T2, typename T3, typename T4>
T*  gc_new_with_init4(gc_mgr_t *mgr, T1 const &val1, T2 const &val2, T3 const &val3,  T4 const &val4, bool add_to_tail=true)
{
    GC_NEW_WITH_INIT_PRE_EXPR(1);

    ::new (p->data) T(val1, val2, val3, val4);

    GC_NEW_WITH_INIT_POST_EXPR(1);
}

template <typename T, typename T1, typename T2, typename T3, typename T4, typename T5>
T*  gc_new_with_init5(gc_mgr_t *mgr, T1 const &val1, T2 const &val2, T3 const &val3,  T4 const &val4, T5 const & val5, bool add_to_tail=true)
{
    GC_NEW_WITH_INIT_PRE_EXPR(1);

    ::new (p->data) T(val1, val2, val3, val4, val5);

    GC_NEW_WITH_INIT_POST_EXPR(1);
}


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



}

#endif /* end of include guard */
