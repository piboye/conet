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
 *         Author:  YOUR NAME (),
 *   Organization:
 *
 * =====================================================================================
 */
#include <stdlib.h>
#include "dispatch.h"
#include "base/incl/tls.h"
#include "base/incl/time_helper.h"
#include "thirdparty/gflags/gflags.h"
#include "coroutine.h"

namespace conet
{

struct dispatch_mgr_t
{
    list_head tasks;
    list_head delay_tasks;
    void init()
    {
        INIT_LIST_HEAD(&tasks);
        INIT_LIST_HEAD(&delay_tasks);
    }
};


void free_dispach_mgr(dispatch_mgr_t * mgr)
{
    task_t *t=NULL, *n = NULL;
    list_for_each_entry_safe(t, n, &mgr->tasks, link_to) {
        list_del_init(&t->link_to);
        if (t->auto_del) {
            free(t);
        }
    }

    list_for_each_entry_safe(t, n, &mgr->delay_tasks, link_to) {
        list_del_init(&t->link_to);
        if (t->auto_del) {
            free(t);
        }
    }
    
    free(mgr);
}

dispatch_mgr_t *alloc_dispatch_mgr()
{
    dispatch_mgr_t *mgr = (dispatch_mgr_t *)malloc(sizeof(dispatch_mgr_t));
    mgr->init();
    return mgr;
}

static __thread dispatch_mgr_t * g_dispatch_mgr;
CONET_DEF_TLS_VAR_HELP(g_dispatch_mgr, ({alloc_dispatch_mgr();}), ({free_dispach_mgr(self);}));

int proc_tasks(dispatch_mgr_t *mgr)
{
    int num = 0;
    task_t *t=NULL, *n = NULL;
    list_for_each_entry_safe(t, n, &mgr->tasks, link_to) {
        int ret = t->proc(t->arg);
        if(ret >0) num+=ret;
    }

    if (list_empty(&mgr->delay_tasks)) {
        return num;
    }

    list_head delay_list;
    { // swap delay list;
        INIT_LIST_HEAD(&delay_list);
        list_add(&delay_list, &mgr->delay_tasks);
        list_del_init(&mgr->delay_tasks);
    }

    list_for_each_entry_safe(t, n, &delay_list, link_to) {
        list_del_init(&t->link_to);
        int ret = t->proc(t->arg);
        if(ret >0) num+=ret;
        if (t->auto_del) {
            free(t);
        }
    }

    return num;
}


int dispatch()
{
    return proc_tasks(TLS_GET(g_dispatch_mgr));
}

void registry_task(list_head *list, task_t *task)
{
    list_add_tail(&task->link_to, list);
}

void registry_task(task_t *task)
{
    dispatch_mgr_t *mgr = TLS_GET(g_dispatch_mgr);
    if (list_empty(&task->link_to)) {
        list_add_tail(&task->link_to, &mgr->tasks);
    } else {
        list_move_tail(&task->link_to, &mgr->tasks);
    }
}

void unregistry_task(task_t *task)
{
    list_del_init(&task->link_to);
}

void init_task(task_t *task, task_proc_func_t proc, void *arg)
{
    task->proc= proc;
    task->arg = arg;
    task->auto_del = 0;
    INIT_LIST_HEAD(&task->link_to);
}

void registry_task(list_head *list, task_proc_func_t proc, void *arg)
{
    task_t * t = (task_t *) malloc(sizeof(task_t));
    init_task(t, proc, arg);
    t->auto_del = 1;
    registry_task(list, t);
}

void registry_task(task_proc_func_t proc, void *arg)
{
    registry_task(&TLS_GET(g_dispatch_mgr)->tasks, proc, arg);
}

void registry_delay_task(task_t *task)
{
    if (list_empty(&task->link_to)) {
        list_add_tail(&task->link_to, &TLS_GET(g_dispatch_mgr)->delay_tasks);
    }else {
        list_move_tail(&task->link_to, &TLS_GET(g_dispatch_mgr)->delay_tasks);
    }
}

struct delay_back_t
{
    task_t task;
    coroutine_t *co;
};

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
