#ifndef __CONET_COROUTINE_H_INC__
#define __CONET_COROUTINE_H_INC__
#include <stdlib.h>
#include <stdint.h>
#include <poll.h>
#include "base/list.h"
#include "gc.h"
#include "base/closure.h"

namespace conet
{


struct coroutine_t;

// coroutine enviroment, one env per thread
struct coroutine_env_t;

typedef int CO_MAIN_FUN(void *);
typedef int (*co_main_func_t)(void *);


coroutine_t * alloc_coroutine(CO_MAIN_FUN * fn, void * arg, uint32_t stack_size=0);

void free_coroutine(coroutine_t *co);

int init_coroutine(coroutine_t * self);
int set_callback(coroutine_t * self, CO_MAIN_FUN * fn, void * arg, int stack_size);

coroutine_t * current_coroutine();

#define CO_SELF() conet::current_coroutine()

void * yield(list_head *wait_to = NULL, void *val=NULL) ;

void * resume(coroutine_t * co, void *val=NULL);

coroutine_env_t * get_coroutine_env();

void set_coroutine_desc(coroutine_t *co, char const *desc);
void set_auto_delete(coroutine_t *co);

int get_epoll_pend_task_num();

int co_poll(struct pollfd fds[], nfds_t nfds, int timeout);
void * get_yield_value(coroutine_t *co);

void print_stacktrace(coroutine_t *co, int fd=2);

// 协程的spec 变量， 最好用 CO_NEW 来创建, 这样可以自动回收
void * get_spec(uint64_t key);
int set_spec(uint64_t key, void * val);
uint64_t create_spec_key();

void * get_pthread_spec(pthread_key_t key);
int set_pthread_spec(pthread_key_t key, const void * val);

bool is_stop(coroutine_t *co);

int wait(coroutine_t *co);
int wait(coroutine_t *co, uint32_t ms);

uint64_t set_timeout(void (*fn)(void *), void *, int ms, int stack_size = 0);
uint64_t set_interval(void (*fn)(void *), void *, int ms, int stack_size = 0);

uint64_t set_timeout(closure_t<void> *cl, int ms, int stack_size=0);
uint64_t set_interval(closure_t<void> *cl, int ms, int stack_size=0);

void cancel_timeout(uint64_t);
void cancel_interval(uint64_t);

class Coroutine
{
public:
    coroutine_t *m_co;

    static int CallRun(void * arg)
    {
        return ((Coroutine *)(arg))->run();
    }

    Coroutine()
    {
        m_co = alloc_coroutine(&CallRun, this);
    }

    void resume()
    {
        conet::resume(m_co);
    }

    void yield()
    {
        conet::yield();
    }

    virtual ~Coroutine()
    {
        free_coroutine(m_co);
    }

    virtual int run()=0;
};

int start_gettimeofday_improve(int ms);
int stop_gettimeofday_improve();

}

#endif //
