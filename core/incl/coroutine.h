#ifndef __CONET_COROUTINE_H_INC__
#define __CONET_COROUTINE_H_INC__
#include <stdlib.h>
#include <poll.h>
#include "base/incl/list.h"
#include "gc.h"
#include "timewheel.h"
#include "base/incl/closure.h"

namespace conet
{


struct coroutine_t;

// coroutine enviroment, one env per thread
struct coroutine_env_t;

typedef int CO_MAIN_FUN(void *);


coroutine_t * alloc_coroutine(CO_MAIN_FUN * fn, void * arg,  \
                              uint32_t stack_size=0, coroutine_env_t * env=NULL);

void free_coroutine(coroutine_t *co);

int init_coroutine(coroutine_t * self, CO_MAIN_FUN * fn, void * arg,  \
                   int stack_size, coroutine_env_t *a_env);

coroutine_t * current_coroutine();

#define CO_SELF() current_coroutine()

void * yield(list_head *wait_to = NULL, void *val=NULL) ;

void * resume(coroutine_t * co, void *val=NULL);

coroutine_env_t * get_coroutine_env();

void set_coroutine_desc(coroutine_t *co, char const *desc);
void set_auto_delete(coroutine_t *co);

int get_epoll_pend_task_num();

int co_poll(struct pollfd fds[], nfds_t nfds, int timeout);

void print_stacktrace(coroutine_t *co, int fd=2);

// coroutine gc memory alloctor
gc_mgr_t *get_gc_mgr();

// like new ClassA();
#define CO_NEW(type) conet::gc_new<type>(1, conet::get_gc_mgr())


// like new ClassA(val);
#define CO_NEW_WITH(type, val) conet::gc_new_with_init<type>(val, conet::get_gc_mgr())

// like new ClassA[num];
#define CO_NEW_ARRAY(type, num) conet::gc_new<type>(num, conet::get_gc_mgr())

// like (TypeA *) malloc(sizeof(TypeA))
#define CO_ALLOC(type) conet::gc_alloc<type>(1, conet::get_gc_mgr())

// like (TypeA *) malloc(num * sizeof(TypeA))
#define CO_ALLOC_ARRAY(type, num) conet::gc_alloc<type>(num, conet::get_gc_mgr())

#define MAKE_GC_LEVEL() \
    conet::ScopeGC gc_scope_level_ ## __LINE__

void * get_static_var(void *key);
void * set_static_var(void * key, void *val);



#define CO_DEF_STATIC_VAR(type, name, init_val) \
    static int co_static_var_ct_ ## name = 0; \
    type * co_static_var_p_ ## name =  (type *) get_static_var(& co_static_var_ct_ ## name); \
    if (co_static_var_p_ ## name == NULL) { \
        co_static_var_p_ ## name = gc_new_with_init<type>(init_val, get_gc_mgr(), false); \
        set_static_var(&co_static_var_ct_ ## name, co_static_var_p_ ## name); \
    } \
    type & name = * co_static_var_p_ ## name;

#define CO_DEF_STATIC_PTR(type, name, init_val) \
    static int co_static_var_ct_ ## name = 0; \
    type * name =  (type *) get_static_var(& co_static_var_ct_ ## name); \
    if (name == NULL) { \
        name = init_val; \
        set_static_var(&co_static_var_ct_ ## name, name); \
    } \
 
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

uint64_t set_timeout(Closure<void> *cl, int ms, int stack_size=0);
uint64_t set_interval(Closure<void> *cl, int ms, int stack_size=0);

void cancel_timeout(uint64_t);
void cancel_interval(uint64_t);

//
template<typename T>
int co_mem_fun_helper(void * arg)
{
    T *self = (T *) arg;
    return self->run();
}

class Coroutine
{
public:
    coroutine_t *m_co;

    Coroutine()
    {
        m_co = alloc_coroutine(&co_mem_fun_helper<Coroutine>, this);
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

}

#endif //
