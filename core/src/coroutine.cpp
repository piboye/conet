#include "coroutine.h"
#include "coroutine_impl.h"
#include <pthread.h>
#include <unistd.h>
#include <assert.h>
#include <sys/syscall.h>
#include "tls.h"
#include <stdint.h>
#include <malloc.h>

#define ALLOC_VAR(type) (type *) malloc(sizeof(type))

namespace conet
{


static
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

void delay_del_coroutine(void *arg)
{
    coroutine_t * co = (coroutine_t *)(arg);
    free_coroutine(co);
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

    {
        // notify exit wait queue
        list_head *it=NULL, *next=NULL;
        list_for_each_safe(it, next, &co->exit_notify_queue)
        {
            coroutine_t * co2 = container_of(it, coroutine_t, wait_to);
            list_del_init(it);
            resume(co2, (void *)(uint64_t)(co->ret_val));
        }
    }

    if (co->is_end_delete) {
        //auto delete coroute object;
        //free_coroutine(co);
        timeout_handle_t del_self;
        init_timeout_handle(&del_self, delay_del_coroutine, co, 1);
        set_timeout(&del_self, 1);
        co_return();
        return;
    }
    co_return();
    return ;
}


#define CACHE_LINE_SIZE 64
int init_coroutine(coroutine_t * self, CO_MAIN_FUN * fn, void * arg,  \
                   int stack_size, coroutine_env_t *a_env)
{

    self->ret_val = 0;
    self->is_enable_sys_hook = 0;
    self->is_end_delete = 0;
    self->is_enable_pthread_hook = 0;
    self->is_enable_disk_io_hook = 0;
    self->is_main =0;

    // stack  group from high address to  low; align depend stack_size must be multiplies align size
    //
    stack_size =  (stack_size+CACHE_LINE_SIZE-1)/CACHE_LINE_SIZE*CACHE_LINE_SIZE;
    self->stack = memalign(CACHE_LINE_SIZE, stack_size), 
    self->ctx.uc_stack.ss_sp = self->stack;
    self->ctx.uc_stack.ss_size = stack_size;
    self->ctx.uc_link = NULL;
    self->pfn = fn;
    self->pfn_arg = arg;
    self->state = CREATE;

    INIT_LIST_HEAD(&self->wait_to);
    INIT_LIST_HEAD(&self->exit_notify_queue);

    self->desc = NULL;
    self->gc_mgr = NULL;
    self->static_vars = NULL;
    self->spec = NULL;
    self->pthread_spec = NULL;
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
    if (co->gc_mgr) {
        gc_free_all(co->gc_mgr);
        free(co->gc_mgr);
        co->gc_mgr = NULL;
    }

    delete co->static_vars;

    delete co->spec;

    delete co->pthread_spec;

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
    coroutine_t *cur =  env->curr_co;
    return cur;
}




gc_mgr_t *get_gc_mgr()
{
    coroutine_t *cur = current_coroutine();
    gc_mgr_t *mgr = cur->gc_mgr;
    if (!mgr) {
        mgr = (gc_mgr_t *) malloc(sizeof(gc_mgr_t));
        init_gc_mgr(mgr);
        cur->gc_mgr = mgr;
    }
    return mgr;
}


void * set_static_var(void * key, void *val)
{
    coroutine_t *cur = current_coroutine();
    if (cur->static_vars == NULL) {
        cur->static_vars = new typeof(*cur->static_vars);
    }
    void *old =  (*cur->static_vars)[key];
    (*cur->static_vars)[key] = val;
    return old;
}


void * get_static_var(void *key)
{
    coroutine_t *cur = current_coroutine();
    if (cur->static_vars == NULL) {
        return NULL;
    }
    return (*cur->static_vars)[key];
}

void * get_spec(uint64_t key)
{
    coroutine_t *cur = current_coroutine();
    if (NULL == cur->spec) return NULL;
    return (*cur->spec)[key];
}

int set_spec(uint64_t key, void * val)
{
    coroutine_t *cur = current_coroutine();
    if (NULL == cur->spec) {
        cur->spec = new typeof(*cur->spec);
    }
    (*cur->spec)[key] = val;
    return 0;
}

void * get_pthread_spec(pthread_key_t key)
{
    coroutine_t *cur = current_coroutine();
    if (NULL == cur->pthread_spec) return NULL;
    return (*cur->pthread_spec)[key];
}

int set_pthread_spec(pthread_key_t key, const void * val)
{
    coroutine_t *cur = current_coroutine();
    if (NULL == cur->pthread_spec) {
        cur->pthread_spec = new typeof(*cur->pthread_spec);
    }
    (*cur->pthread_spec)[key] = (void *)val;
    return 0;
}


int wait(coroutine_t *co)
{
    int ret = (uint64_t) (yield(&co->exit_notify_queue, NULL));
    return ret;
}

}

