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
#include "tls.h"
#include "time_helper.h"

namespace conet
{

static __thread list_head * g_tasks = NULL;

void free_task_list(list_head * list)
{
    list_head *it=NULL, *next = NULL;
    list_for_each_safe(it, next, list) {
        task_t * t = container_of(it, task_t, link_to);
        list_del_init(it);
        if (t->auto_del) {
            free(t);
        }
    }
    free(list);
}

list_head *alloc_task_list()
{
    list_head *n = (list_head *)malloc(sizeof(list_head));
    INIT_LIST_HEAD(n);
    return n;
}

DEF_TLS_GET(g_tasks, alloc_task_list(), free_task_list)

int proc_tasks(list_head *list)
{
    int num = 0;
    list_head *it=NULL, *next = NULL;
    list_for_each_safe(it, next, list) {
        task_t * t = container_of(it, task_t, link_to);
        int ret = t->proc(t->arg);
        if(ret >0) num+=ret;
    }
    return num;
}


int proc_netevent(int timeout);

int dispatch(int wait_ms)
{
    conet::update_cached_ms();

    
    int num = 0;

    int cnt = 0;

    for (int i=0; i< 3; ++i) {
        cnt = dispatch_one();
        num+=cnt;
        if (cnt == 0) break;
    }

    if (num >0) wait_ms = 0; 
    cnt = proc_netevent(wait_ms);
    num += cnt;
    return num;
}

int dispatch_one()
{
    return proc_tasks(tls_get(g_tasks));
}

void registry_task(list_head *list, task_t *task)
{
    list_add_tail(&task->link_to, list);
}

void registry_task(task_t *task)
{
    list_add_tail(&task->link_to, tls_get(g_tasks));
}

void unregistry_task(task_t *task)
{
    list_del_init(&task->link_to);
}

void init_task(task_t *task, task_proc_fun_t proc, void *arg)
{
    task->proc= proc;
    task->arg = arg;
    task->auto_del = 0;
    INIT_LIST_HEAD(&task->link_to);
}

void registry_task(list_head *list, task_proc_fun_t proc, void *arg)
{
    task_t * t = (task_t *) malloc(sizeof(task_t));
    init_task(t, proc, arg);
    t->auto_del = 1;
    registry_task(list, t);
}

void registry_task(task_proc_fun_t proc, void *arg)
{
    registry_task(tls_get(g_tasks), proc, arg);
}

}
