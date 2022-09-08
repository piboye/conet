/*
 * =====================================================================================
 *
 *       Filename:  dispatch.h
 *
 *    Description: 任务分发器
 *
 *        Version:  1.0
 *        Created:  2014年05月21日 17时23分51秒
 *       Revision:  none
 *       Compiler:  gcc
 *
 *         Author:  piboye
 *   Organization:
 *
 * =====================================================================================
 */
#ifndef __DISPATCH_H_INC__
#define __DISPATCH_H_INC__

#include <stdint.h>
#include "base/list.h"

namespace conet
{

typedef int (*task_proc_func_t)(void *);
struct task_t
{
    int (*proc)(void *);
    void *arg;
    list_head link_to;
    int auto_del;
};

struct coroutine_env_t;
struct dispatch_mgr_t
{
    //任务
    list_head tasks;
    //延迟任务
    list_head delay_tasks;

    uint32_t task_num;
    uint32_t delay_task_num;
    coroutine_env_t *co_env;

    explicit
    dispatch_mgr_t(coroutine_env_t *env);

    ~dispatch_mgr_t();

    // 注册任务
    void registry(task_t *task);

    //注销任务
    void unregistry(task_t *task);

    // 延迟执行一次任务
    void delay(task_t*);
    
    void unregistry_delay(task_t *task);
};


void init_task(task_t *task, task_proc_func_t proc, void *arg);
void registry_task(list_head *list, task_proc_func_t proc, void *arg);
void registry_task(list_head *list, task_t *task);

void registry_task(task_proc_func_t proc, void *arg);
void registry_task(task_t *task);

void unregistry_task(task_t *task);

void registry_delay_task(task_t *task);
void registry_delay_task(task_proc_func_t proc, void *arg);

int proc_tasks(list_head *list);
int dispatch();

int proc_delay_back(void *arg);
void delay_back();

}

#endif /* end of include guard */
