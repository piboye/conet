/*
 * =====================================================================================
 *
 *       Filename:  dispatch.cpp
 *
 *    Description:
 *
 *        Version:  1.0
 *        Created:  2014年05月21日 17时14分09秒
 *       Revision:  none
 *       Compiler:  gcc
 *
 *         Author:  piboye
 *   Organization:
 *
 * =====================================================================================
 */
#include <stdlib.h>
#include "dispatch.h"
#include "gflags/gflags.h"
#include "coroutine.h"
#include "base/time_helper.h"
#include "coroutine_env.h"

namespace conet
{

dispatch_mgr_t::dispatch_mgr_t(coroutine_env_t *env)
{
    INIT_LIST_HEAD(&tasks);
    INIT_LIST_HEAD(&delay_tasks);
    task_num = 0;
    delay_task_num = 0;
    this->co_env = env;
}

dispatch_mgr_t::~dispatch_mgr_t()
{
    task_t *t=NULL, *n = NULL;
    {
        LIST_HEAD(queue);
        list_swap(&queue, &tasks);
        list_for_each_entry_safe(t, n, &queue, link_to) 
        {
            list_del_init(&t->link_to);
            if (t->auto_del) {
                delete t;
            }
        }
    }

    {
        LIST_HEAD(queue);
        list_swap(&queue, &delay_tasks);
        list_for_each_entry_safe(t, n, &queue, link_to) 
        {
            list_del_init(&t->link_to);
            if (t->auto_del) {
                delete t;
            }
        }
    }
}

void dispatch_mgr_t::registry(task_t *task)
{
    list_head * queue = &this->tasks;
    if (list_empty(&task->link_to)) {
        list_add_tail(&task->link_to, queue);
    } else {
        // 重新注册, 应该报错
        list_move_tail(&task->link_to, queue);
    }

    ++this->task_num;
}

void dispatch_mgr_t::delay(task_t *task)
{
    list_head * queue = &this->delay_tasks;
    if (list_empty(&task->link_to)) {
        list_add_tail(&task->link_to, queue);
        ++this->delay_task_num;
    } else {
        // 重新注册
        //list_move_tail(&task->link_to, queue);
    }
}

void dispatch_mgr_t::unregistry(task_t *task)
{
    list_del_init(&task->link_to);
    --this->task_num;
}

void dispatch_mgr_t::unregistry_delay(task_t *task)
{
    list_del_init(&task->link_to);
    --this->delay_task_num;
}

struct dispatch_mgr_t *get_dispatch_mgr()
{
    return get_coroutine_env()->dispatch;
}

int proc_tasks(dispatch_mgr_t *mgr)
{
    int num = 0;
    task_t *t=NULL, *n = NULL;

    if (mgr->task_num > 0) 
    {
        list_for_each_entry_safe(t, n, &mgr->tasks, link_to) 
        {
            int ret = t->proc(t->arg);
            if(ret >0) ++num;
        }
    }

    if (mgr->delay_task_num <=0) {
        return num;
    }

    LIST_HEAD(delay_list);
    list_swap(&delay_list, &mgr->delay_tasks);

    list_for_each_entry_safe(t, n, &delay_list, link_to) {
        list_del_init(&t->link_to);
        int ret = t->proc(t->arg);
        if(ret >0) ++num;
        if (t->auto_del) {
            delete t;
        }
    }

    return num;
}


int dispatch()
{
    return proc_tasks(get_dispatch_mgr());
}

void registry_task(task_t *task)
{
    dispatch_mgr_t *mgr = get_dispatch_mgr();
    mgr->registry(task);
}

void unregistry_task(task_t *task)
{
    dispatch_mgr_t *mgr = get_dispatch_mgr();
    mgr->unregistry(task);
}

void init_task(task_t *task, task_proc_func_t proc, void *arg)
{
    task->proc= proc;
    task->arg = arg;
    task->auto_del = 0;
    INIT_LIST_HEAD(&task->link_to);
}

void registry_task(task_proc_func_t proc, void *arg)
{
    task_t *task = new task_t();
    init_task(task, proc, arg);
    task->auto_del = 1;
    dispatch_mgr_t *mgr = get_dispatch_mgr();
    mgr->registry(task);
}

void registry_delay_task(task_t *task)
{
    dispatch_mgr_t *mgr = get_dispatch_mgr();
    mgr->delay(task);
}

void registry_delay_task(task_proc_func_t proc, void *arg)
{
    task_t * t = new task_t();
    init_task(t, proc, arg);
    t->auto_del = 1;
    registry_delay_task(t);
}

struct delay_back_t
{
    task_t task;
    coroutine_t *co;
};

static 
int proc_delay_back(void *arg)
{
    delay_back_t * self = (delay_back_t *)(arg);
    conet::resume(self->co);
    return 0;
}

void delay_back()
{
    delay_back_t t;
    init_task(&t.task, &proc_delay_back, &t);
    t.co = CO_SELF();
    registry_delay_task(&t.task);
    conet::yield();
}

}
