/*
 * =====================================================================================
 *
 *       Filename:  coroutine_env.cpp
 *
 *    Description:
 *
 *        Version:  1.0
 *        Created:  2014年05月20日 03时20分07秒
 *       Revision:  none
 *       Compiler:  gcc
 *
 *         Author:  piboyeliu
 *   Organization:
 *
 * =====================================================================================
 */
#include <stdlib.h>
#include <pthread.h>
#include <unistd.h>
#include <assert.h>
#include <sys/syscall.h>
#include <stdint.h>
#include "base/tls.h"
#include "coroutine.h"
#include "coroutine_env.h"
#include "dispatch.h"
#include "timewheel.h"
#include "pthread_hook.h"


namespace conet
{

DEFINE_int32(timewheel_slot_num, 60*1000, "default timewheel slot num");

__thread coroutine_env_t * g_coroutine_env=NULL;

coroutine_env_t ** g_coroutine_envs=NULL;

epoll_ctx_t *create_epoll_ctx(coroutine_env_t *, int event_size);
void free_epoll_ctx(epoll_ctx_t *ep);

int init_coroutine(coroutine_t * self);


int do_delay_del_co_task(void * a)
{
    coroutine_env_t *env = (coroutine_env_t*)(a);
    LIST_HEAD(queue);
    list_swap(&queue, &env->delay_del_list);
    task_t * t=NULL, *n = NULL;
    list_for_each_entry_safe(t, n, &queue, link_to)
    {
        list_del_init(&t->link_to);
        t->proc(t->arg);
    }
    return 0;
}

coroutine_env_t::coroutine_env_t()
{

    coroutine_env_t *self = this;

    self->main = new coroutine_t();
    init_coroutine(self->main);
    self->main->is_main = 1;
    self->main->state = RUNNING;
    self->main->desc = "main";
    INIT_LIST_HEAD(&self->main->wait_to);

    self->curr_co = self->main;
    INIT_LIST_HEAD(&self->run_queue);
    list_add_tail(&self->main->wait_to, &self->run_queue);

    INIT_LIST_HEAD(&self->tasks);
    self->spec_key_seed = 10000;

    this->pthread_mgr = NULL;

    // 初始化任务分发器
    self->dispatch = new dispatch_mgr_t(this);

    INIT_LIST_HEAD(&this->delay_del_list);
    init_task(&delay_del_task, do_delay_del_co_task, this);

    // 初始化 epoll 环境
    self->epoll_ctx = create_epoll_ctx(this, 100);

    // 添加到调度
    self->dispatch->registry(&this->epoll_ctx->task);

    // 初始化定时器
    self->tw = new timewheel_t(this, FLAGS_timewheel_slot_num);
    //后续才能启动, 因为全局变量为设置 
    // self->tw->start();


    //堆栈设置为 64 字节对齐
    this->default_stack_pool.init(FLAGS_stack_size, 100000, 64);

    this->co_struct_pool.init(sizeof(coroutine_t), 1000, __alignof__(coroutine_t));

}



coroutine_env_t::~coroutine_env_t()
{
    if (this->pthread_mgr) {
        free_pthread_mgr(this->pthread_mgr);
        this->pthread_mgr = NULL;
    }

    this->tw->stop(100);
    delete this->tw;
    this->tw = NULL;

    // 注销epoll 调度
    this->dispatch->unregistry(&this->epoll_ctx->task);
    free_epoll_ctx(this->epoll_ctx);
    this->epoll_ctx = NULL;


    delete this->dispatch;
    this->dispatch = NULL;

    do_delay_del_co_task(this);

    delete this->main;
    this->main = NULL;

    this->default_stack_pool.fini();
    this->co_struct_pool.fini();
}


int proc_netevent(int timeout);

/*
int dispatch_one(int timeout)
{
    coroutine_env_t * env = get_coroutine_env();
    assert(env);
    //check_timewheel(&env->tw);
    proc_tasks(&env->tasks);
    proc_netevent(timeout);
    return 0;
}
*/

uint64_t create_spec_key()
{
    coroutine_env_t * env = get_coroutine_env();
    return env->spec_key_seed++;
}

}
