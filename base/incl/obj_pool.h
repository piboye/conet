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

template <typename obj_t>
class ObjPool
{
public:
    typedef obj_t obj_type;


    Lifo m_queue;

    obj_type * (*m_alloc_func)(void *);
    void * m_alloc_arg;

    obj_type * (*m_alloc_func2)();

    explicit
    ObjPool()
    {

        m_alloc_func = NULL;
        m_alloc_arg = NULL; 
        m_alloc_func2 = NULL;
        m_queue.set_delete_obj_func(&delete_obj_help, NULL);
    }

    static
    int delete_obj_help(void *arg, void *obj)
    {
        obj_type *v = (obj_type *)(obj);
        delete v;
        return 0;
    }

    ~ObjPool() 
    {
    }

    int init(obj_type * (*alloc_func)(void *), void *arg=NULL)
    {
        m_alloc_func = alloc_func;
        m_alloc_arg = arg;
        return 0;
    }

    int init ()
    {
        m_alloc_func2 = &new_obj;
        return 0;
    }

    static obj_type *new_obj()
    {
        return new obj_type();
    }


    obj_type * alloc() 
    {
        if (m_queue.empty())  {
            if (m_alloc_func) {
                return m_alloc_func(m_alloc_arg);
            } 
            if (m_alloc_func2) {
                return m_alloc_func2();
            }
            return NULL;
        }
        obj_type * obj = NULL;
        obj = (obj_type *)m_queue.pop();
        return obj;
    }

    void release(obj_type * obj) 
    {
        m_queue.push(obj);
    }

};

}

#endif /* end of include guard */
