/*
 * =====================================================================================
 *
 *       Filename:  barrier.h
 *
 *    Description:  
 *
 *        Version:  1.0
 *        Created:  03/11/2015 09:35:55 PM
 *       Revision:  none
 *       Compiler:  gcc
 *
 *         Author:  piboye
 *   Organization:  
 *
 * =====================================================================================
 */
#ifndef __CONET_BARRIER_H__
#define __CONET_BARRIER_H__

#include "base/closure.h"


namespace conet
{

    struct parallel_task_t
    {
        list_head link_to;
        coroutine_t * co;
        closure<int, void*> * proc;
        parallel_task_t()
        {
            INIT_LIST_HEAD(&link_to);
            co = NULL;
            proc = NULL;
        }
    };

    struct parallel_controll_t
    {
        list_head tasks;
        int task_num;       //总任务数
        int finished_num;  //完成任务数
        parallel_controll_t()
        {
            INIT_LIST_HEAD(&tasks);
            task_num = 0;
            finished_num = 0;
        }

        void add_task(parallel_task_t *task)
        {
            list_add_tail(&task->link_to, &this->tasks);
            ++this->task_num;
        }

        int wait_all()
        {
            parallel_task_t * task=NULL, *n = NULL;
            list_for_each_entry_safe(task, n, &tasks, link_to)
            {
                conet::wait(task->co);
                conet::free_coroutine(task->co);
                delete task->proc;
                list_del(task->link_to);
                ++finished_num;
            }
        }
    };

#define BEGIN_PARALLEL \
    list_head __parallel_node;

#define DO_PARALLEL(ref_vars, op)   \
    do { \
        conet::parallel_task_t parallel_task_##__LINE__(__parallel_node); \
        __parallel_task_##__LINE__.proc = (void *) NewClsureWithRef( \
                int,  \
                (void * __co_self__), ref_vars,  \
                    { \
                     op \
                    return 0; \
                    } \
                ); \
        __parallel_task_##__LINE__.co = alloc_coroutine( \
                conet::call_closure<int>, \
                __parallel_task_##__LINE__.proc); \
        __parallel_node.add(&__parallel_task_##__LINE__); \
        conet::resume(__parallel_task_##__LINE__.co, __parallel_task_##__LINE__.proc) ;\
    }while(0);

#define END_PARALLEL   \
    __parallel_node.wait_all()

}


/*
 * usage : 
 
  BEGIN_PARALLEL
  {
    DO_PARALLEL((local_var1, local_var2), {
        do_rpc_requst1();
    });

    DO_PARALLEL((local_var3), {
        do_rpc_requst2();
    });
  }
  END_PARALLEL;

*/

}


#endif /* end of include guard */
