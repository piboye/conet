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
#include <stdint.h>
#include "../../base/incl/tls.h"


namespace conet
{

int init_coroutine(coroutine_t * self);
void init_coroutine_env(coroutine_env_t *self)
{

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
}

__thread coroutine_env_t * g_coroutine_env=NULL;

coroutine_env_t *alloc_coroutine_env()
{
    coroutine_env_t *env = new coroutine_env_t();
    init_coroutine_env(env);
    return env;
}


void free_coroutine_env(void *arg)
{
    coroutine_env_t *env = (coroutine_env_t *)arg;
    delete env->main;
    delete(env);
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
