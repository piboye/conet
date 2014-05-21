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

namespace conet
{


static list_head * g_tasks = NULL;

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
        t->proc(t->arg);
        ++num;
    }
    return num;
}


int dispatch_one() 
{
    return proc_tasks(tls_get(g_tasks));
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
}

void registry_task(task_proc_fun_t proc, void *arg)
{
    task_t * t = (task_t *) malloc(sizeof(task_t));
    init_task(t, proc, arg);
    t->auto_del = 1;
    registry_task(t);
}

}
