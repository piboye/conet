#ifndef __CONET_COROUTINE_H_INC__
#define __CONET_COROUTINE_H_INC__
#include <stdlib.h>
#include <poll.h>
#include "list.h"
#include "gc.h"

namespace conet
{

struct coroutine_t;

// coroutine enviroment, one env per thread
struct coroutine_env_t;

typedef int CO_MAIN_FUN(void *);


coroutine_t * alloc_coroutine(CO_MAIN_FUN * fn, void * arg,  \
        int stack_size=128*1024, coroutine_env_t * env=NULL);

void free_coroutine(coroutine_t *co);

coroutine_t * current_coroutine();

void * yield(list_head *wait_to = NULL, void *val=NULL) ;

void * resume(coroutine_t * co, void *val=NULL);

coroutine_env_t * get_coroutine_env();

void set_coroutine_desc(coroutine_t *co, char const *desc);
void set_auto_delete(coroutine_t *co);

int is_enable_sys_hook();
void disable_sys_hook();
void enable_sys_hook();
int get_epoll_pend_task_num(); 
int epoll_once(int timeout);
int co_poll(struct pollfd fds[], nfds_t nfds, int timeout);

// coroutine gc memory alloctor
gc_mgr_t *get_gc_mgr();

#define CO_NEW(type) gc_new<type>(1, get_gc_mgr())

#define CO_NEW_ARRAY(type, num) gc_new<type>(num, get_gc_mgr())

#define CO_ALLOC(type) gc_alloc<type>(1, get_gc_mgr())

#define CO_ALLOC_ARRAY(type, num) gc_alloc<type>(num, get_gc_mgr())

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
