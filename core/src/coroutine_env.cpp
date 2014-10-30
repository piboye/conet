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
#include "coroutine.h"
#include "coroutine_impl.h"
#include <pthread.h>
#include <unistd.h>
#include <assert.h>
#include <sys/syscall.h>
#include "base/incl/tls.h"
#include <stdint.h>

#define ALLOC_VAR(type) (type *) malloc(sizeof(type))

namespace conet
{

void init_coroutine_env(coroutine_env_t *self)
{
    self->default_stack_pool.init(FLAGS_stack_size, 10000, 64); // 64 bytes for cache_line align

    self->main = ALLOC_VAR(coroutine_t);
    init_coroutine(self->main, NULL, NULL, 128*1024, NULL);
    self->main->is_main = 1;
    self->main->state = RUNNING;
    self->main->desc = "main";
    getcontext(&self->main->ctx);
    //makecontext(&self->main->ctx, NULL, 0);
    self->curr_co = self->main;
    INIT_LIST_HEAD(&self->run_queue);
    list_add_tail(&self->main->wait_to, &self->run_queue);

    INIT_LIST_HEAD(&self->tasks);


    self->spec_key_seed = 10000;
}

static __thread coroutine_env_t * g_env=NULL;

inline
coroutine_env_t *alloc_coroutine_env()
{
    coroutine_env_t *env = ALLOC_VAR(coroutine_env_t);
    init_coroutine_env(env);
    return env;
}


void free_coroutine_env(void *arg)
{
    coroutine_env_t *env = (coroutine_env_t *)arg;
    free_coroutine(env->main);
    env->default_stack_pool.fini();  // free stack pool
    free(env);
}

DEF_TLS_GET(g_env, alloc_coroutine_env(), free_coroutine_env);

coroutine_env_t * get_coroutine_env()
{
    return tls_get(g_env);
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
