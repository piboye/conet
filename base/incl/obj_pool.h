/*
 * =====================================================================================
 *
 *       Filename:  obj_pool.h
 *
 *    Description:  
 *
 *        Version:  1.0
 *        Created:  2014年09月17日 08时34分33秒
 *       Revision:  none
 *       Compiler:  gcc
 *
 *         Author:  piboyeliu
 *   Organization:  
 *
 * =====================================================================================
 */

#ifndef OBJ_POOL_H_INC
#define OBJ_POOL_H_INC

#include "lifo.h"

namespace conet
{

struct obj_pool_t
    :public Lifo
{
    void * (*m_alloc_func)(void *);
    void * m_alloc_arg;

    obj_pool_t() 
    {
        m_alloc_func = NULL;
        m_alloc_arg = NULL;
    }

    void set_alloc_obj_func( void *(*fn)(void *arg), void *arg)
    {
        m_alloc_func = fn;
        m_alloc_arg = arg;
    }

    void * alloc() 
    {
        void * obj = pop();
        if (NULL == obj) {
            if (m_alloc_func) {
                return m_alloc_func(m_alloc_arg);
            } 
        }
        return obj;
    }

    void release(void * obj) 
    {
        push(obj);
    }
};

template <typename obj_t>
class ObjPool :public obj_pool_t
{
public:
    typedef obj_t obj_type;

    explicit
    ObjPool()
    {
        set_alloc_obj_func(new_obj_help, NULL);
        set_free_obj_func(delete_obj_help, NULL);
    }

    static
    void delete_obj_help(void *arg, void *obj)
    {
        obj_type *v = (obj_type *)(obj);
        delete v;
    }

    static void *new_obj_help(void *arg)
    {
        return new obj_type();
    }

    ~ObjPool() 
    {
    }

    obj_type * alloc() 
    {
        return (obj_type *)obj_pool_t::alloc();
    }

    void release(obj_type * obj) 
    {
        obj_pool_t::release(obj);
    }

};

}

#endif /* end of include guard */
