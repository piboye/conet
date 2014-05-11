#include "coroutine.h"
#include "coroutine_impl.h"
#include <pthread.h>
#include <unistd.h>
#include <assert.h>
#include <sys/syscall.h>
#include "tls.h"
#include <stdint.h>

#define ALLOC_VAR(type) (type *) malloc(sizeof(type))

namespace conet
{

void co_return(void *val=NULL) {
    coroutine_env_t *env = get_coroutine_env();
    if (list_empty(&env->run_queue)) {
        assert(!"co thread env run queue empty");
        return ;
    }

    coroutine_t *last = container_of(env->run_queue.prev, coroutine_t, wait_to);
    list_del_init(&last->wait_to);
    env->curr_co = last;
    last->state = RUNNING;
    last->yield_val = val;
    setcontext(&last->ctx);
}

static
void co_main_helper(int co_low, int co_high )
{
    unsigned long p = (uint32_t)co_high;
    p <<= 32;
    p |= (uint32_t)co_low;

    coroutine_t *co = (coroutine_t *)p;

    //run main proc
    co->ret_val = co->pfn(co->pfn_arg);

    co->state = STOP;
    list_del_init(&co->wait_to);
    coroutine_env_t *env = get_coroutine_env();
    assert(env->curr_co == co);
    env->curr_co = container_of(co->ctx.uc_link, coroutine_t, ctx);
    list_del_init(&env->curr_co->wait_to);
    
    if (co->is_end_delete) {
        //auto delete coroute object;
        free_coroutine(co);
    }
    co_return(); 
    return ;    
}



int init_coroutine(coroutine_t * self, CO_MAIN_FUN * fn, void * arg,  \
        int stack_size, coroutine_env_t *a_env)
{

    self->ret_val = 0;
    self->is_enable_sys_hook = 0;
    self->is_end_delete = 0;
    self->is_main =0;

    self->stack = malloc(stack_size);
    self->ctx.uc_stack.ss_sp = self->stack;
    self->ctx.uc_stack.ss_size = stack_size;
    self->ctx.uc_link = NULL;
    self->pfn = fn;
    self->pfn_arg = arg;
    self->state = CREATE;
    self->desc = NULL;
    INIT_LIST_HEAD(&self->wait_to);
    return 0;
}

void set_auto_delete(coroutine_t *co) 
{
    co->is_end_delete = 1;
}

void set_coroutine_desc(coroutine_t *co, char const *desc)
{
    co->desc = desc;
}

coroutine_t * alloc_coroutine(CO_MAIN_FUN * fn, void * arg,  \
        int stack_size, coroutine_env_t * env)
{
    coroutine_t *co = ALLOC_VAR(coroutine_t);
    init_coroutine(co, fn, arg, stack_size, env); 
    return co;
}

void free_coroutine(coroutine_t *co) 
{
   assert(co);
   free(co->stack);
   free(co);
}


typedef void (*coroutine_fun_t)();

void *resume(coroutine_t * co, void * val)
{
    coroutine_env_t *env = get_coroutine_env();
    coroutine_t *cur = env->curr_co;
    assert(cur);
    if (CREATE == co->state) {
        uint64_t p = (uint64_t) co;
        getcontext(&co->ctx);
        makecontext(&co->ctx, (coroutine_fun_t) co_main_helper, 2, \
                (uint32_t)(p & 0xffffffff), (uint32_t)((p >> 32) & 0xffffffff) );
    }
    co->ctx.uc_link = &cur->ctx;
    co->state = RUNNING;
    list_del_init(&co->wait_to);
    env->curr_co = co;
    list_add_tail(&cur->wait_to, &env->run_queue);
    co->yield_val = val;
    swapcontext(&(cur->ctx), &(co->ctx) );
    return cur->yield_val;
}

void * yield(list_head *wait_to, void * val) 
{
    coroutine_env_t *env = get_coroutine_env();
    coroutine_t *cur = env->curr_co;

    if (list_empty(&env->run_queue)) {
        assert(!"co thread env run queue empty");
        return NULL;
    }

    //assert(cur->ctx.uc_link);
    coroutine_t *last = container_of(env->run_queue.prev, coroutine_t, wait_to);
    
    list_del_init(&last->wait_to);
    list_del_init(&cur->wait_to);

    env->curr_co = last;

    if (wait_to) { 
        list_add_tail(&cur->wait_to, wait_to);
    } 

    cur->state = SUSPEND;
    last->state = RUNNING;
    last->yield_val = val;
    swapcontext(&cur->ctx, &last->ctx);
    return cur->yield_val;
}

coroutine_t * current_coroutine() 
{
    coroutine_env_t * env = get_coroutine_env();
    coroutine_t *cur =  env->curr_co; //container_of(env->run_queue.prev, coroutine_t, wait_to);
    return cur;
}


void init_coroutine_env(coroutine_env_t *self) 
{
    self->main = ALLOC_VAR(coroutine_t);
    init_coroutine(self->main, NULL, NULL, 128*1024, self);
    self->main->is_main = 1;
    self->main->state = RUNNING;
    self->main->desc = "main";
    getcontext(&self->main->ctx);
    makecontext(&self->main->ctx, NULL, 0);
    self->curr_co = self->main;
    INIT_LIST_HEAD(&self->run_queue);
    list_add_tail(&self->main->wait_to, &self->run_queue);
    self->epoll_ctx = NULL;
}


static __thread coroutine_env_t * g_env=NULL; 

struct epoll_ctx_t;

void free_epoll(epoll_ctx_t *ep) __attribute__((weak));
void free_coroutine_env(void *arg)
{
    coroutine_env_t *env = (coroutine_env_t *)arg;
    free_coroutine(env->main);
    if (env->epoll_ctx) free_epoll((epoll_ctx_t *)env->epoll_ctx);
    free(env);
}

coroutine_env_t * get_coroutine_env()
{
    if (NULL == g_env) 
    {
        g_env = ALLOC_VAR(coroutine_env_t);
        init_coroutine_env(g_env);
        tls_onexit_add(g_env, free_coroutine_env);
    }
    return g_env; 
}


int is_enable_sys_hook()
{
    if (NULL == g_env) return 0;
    return current_coroutine()->is_enable_sys_hook;
}

void enable_sys_hook()
{
    current_coroutine()->is_enable_sys_hook = 1;
}

void disable_sys_hook()
{
    current_coroutine()->is_enable_sys_hook = 0;
}

}

