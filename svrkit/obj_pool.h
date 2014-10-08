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

#include "core/incl/closure.h"

#include <queue>

namespace conet
{

template <typename obj_t>
class ObjPoll
{
public:
    typedef obj_t obj_type;
    std::queue<obj_type *> m_queue;
    Closure<obj_type *, void> * m_alloc_func;

    explicit
    ObjPoll()
    {

        m_alloc_func = NULL;
    }

    ~ObjPoll() 
    {
        while (!m_queue.empty()) {
            delete m_queue.front();
            m_queue.pop();
        }
        if (m_alloc_func) {
            delete m_alloc_func;
        }
    }

    int init(Closure<obj_type *, void> * alloc_func) 
    {
        m_alloc_func = alloc_func;
        return 0;
    }

    obj_type * alloc() 
    {
        if (m_queue.empty() && m_alloc_func) {
            return m_alloc_func->Run();
        }
        obj_type * obj = NULL;
        obj = m_queue.front();
        m_queue.pop();
        return obj;
    }

    void release(obj_type * obj) 
    {
        m_queue.push(obj);
    }

};

}

#endif /* end of include guard */
