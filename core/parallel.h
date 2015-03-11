/*
 * =====================================================================================
 *
 *       Filename:  parallel.h
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
  WAIT_ALL_PARALLEL();

*/

#ifndef __CONET_PARALEL_H__
#define __CONET_PARALEL_H__

#include "base/closure.h"
#include "base/list.h"


namespace conet
{

    struct parallel_task_t
    {
        list_head link_to;
        coroutine_t * co;
        closure_t<int> * proc;
        parallel_task_t()
        {
            INIT_LIST_HEAD(&link_to);
            co = NULL;
            proc = NULL;
        }
        ~parallel_task_t()
        {
            if (this->co) {
                conet::wait(this->co);
                conet::free_coroutine(this->co);
            }
            delete this->proc;
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

        ~parallel_controll_t()
        {
            parallel_task_t * task=NULL, *n = NULL;
            list_for_each_entry_safe(task, n, &tasks, link_to)
            {
                list_del_init(&task->link_to);
                delete task;
                ++finished_num;
            }
        }
    };

#define BEGIN_PARALLEL \
    do { \
    conet::parallel_controll_t __parallel_node_##__LINE__;


#define DO_PARALLEL(ref_vars, op)   \
    do { \
        conet::parallel_task_t *__parallel_task_##__LINE__ = new conet::parallel_task_t(); \
        __parallel_task_##__LINE__->proc =  NewClosureWithCopy( \
            int, (), ref_vars,  \
                { \
                 op \
                return 0; \
                } \
            ); \
        __parallel_task_##__LINE__->co = conet::alloc_coroutine(\
                conet::call_closure<int>, \
                (void *)__parallel_task_##__LINE__->proc); \
        __parallel_node_##__LINE__.add_task(__parallel_task_##__LINE__); \
        conet::resume(__parallel_task_##__LINE__->co, __parallel_task_##__LINE__->proc) ;\
    } while(0)

//__parallel_node_##__LINE__.wait_all(); 

#define WAIT_ALL_PARALLEL()\
    } while(0)



}


#endif /* end of include guard */
