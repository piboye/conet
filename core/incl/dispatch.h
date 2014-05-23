/*
 * =====================================================================================
 *
 *       Filename:  dispatch.h
 *
 *    Description:
 *
 *        Version:  1.0
 *        Created:  2014年05月21日 17时23分51秒
 *       Revision:  none
 *       Compiler:  gcc
 *
 *         Author:  YOUR NAME (),
 *   Organization:
 *
 * =====================================================================================
 */
#ifndef __DISPATCH_H_INC__
#define __DISPATCH_H_INC__
#include "list.h"

namespace conet
{

typedef int (*task_proc_fun_t)(void *);
struct task_t
{
    int (*proc)(void *);
    void *arg;
    list_head link_to;
    int auto_del;
};


void init_task(task_t *task, task_proc_fun_t proc, void *arg);
void registry_task(task_proc_fun_t proc, void *arg);

void registry_task(task_t *task);

void unregistry_task(task_t *task);


int dispatch_one();

}

#endif /* end of include guard */
