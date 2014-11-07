#include <pthread.h>
#include <unistd.h>
#include <assert.h>
#include <sys/syscall.h>
#include <stdint.h>
#include <malloc.h>
#include <sys/mman.h>

#include "coroutine.h"
#include "coroutine_impl.h"
#include "timewheel.h"
#include "gflags/gflags.h"
#include "log.h"
#include "../../base/incl/fixed_mempool.h"
#include "../../base/incl/tls.h"

DEFINE_int32(stack_size, 128*1024, "default stack size bytes");

extern "C" void conet_swapcontext(ucontext_t *co, ucontext_t *co2);
extern "C" void conet_setcontext(ucontext_t *co);


static __thread conet::fixed_mempool_t * g_coroutine_struct_pool = NULL;

static 
void free_co_struct_pool(void *arg)
{
    conet::fixed_mempool_t * pool = (conet::fixed_mempool_t *)(arg);
    pool->fini();
    delete pool;
}

namespace conet
{

conet::fixed_mempool_t * get_co_struct_pool()
{
    if (NULL == g_coroutine_struct_pool) {
         g_coroutine_struct_pool = new conet::fixed_mempool_t();
         g_coroutine_struct_pool->init(sizeof(coroutine_t), 1000, __alignof__(coroutine_t));
         conet::tls_onexit_add(g_coroutine_struct_pool, &free_co_struct_pool);
    }
    return g_coroutine_struct_pool;
}


void * get_yield_value(coroutine_t *co)
{
    return co->yield_val;
}

bool is_stop(coroutine_t *co)
{
    return co->state == STOP;
}

static
void co_return(void *val=NULL) {
    coroutine_env_t *env = get_coroutine_env();
    if (list_empty(&env->run_queue)) {
        LOG(FATAL)<<"co thread env run queue empty";
        return ;
    }

    coroutine_t *last = container_of(env->run_queue.prev, coroutine_t, wait_to);
    list_del_init(&last->wait_to);
    coroutine_t * curr_co = env->curr_co;
    env->curr_co = last;
    last->state = RUNNING;
    last->yield_val = val;
    //conet_setcontext(&last->ctx);
    conet_swapcontext(&(curr_co->ctx), &(last->ctx));
}

void delay_del_coroutine(void *arg)
{
    coroutine_t * co = (coroutine_t *)(arg);
    free_coroutine(co);
}

static
void co_main_helper2(void *, void *);

static
void co_main_helper(int co_low, int co_high )
{
    uint64_t p = (uint32_t)co_high;
    p <<= 32;
    p |= (uint32_t)co_low;
    co_main_helper2((void *)(p), NULL);
}

int64_t g_page_size  = sysconf(_SC_PAGESIZE);

static
void co_main_helper2(void *p, void *p2)
{
    coroutine_t *co = (coroutine_t *)p;

    //run main proc
    co->ret_val = co->pfn(co->pfn_arg);

    co->state = STOP;
    list_del_init(&co->wait_to);
    coroutine_env_t *env = get_coroutine_env();
    assert(env->curr_co == co);
    //env->curr_co = container_of(co->ctx.uc_link, coroutine_t, ctx);
    list_del_init(&env->curr_co->wait_to);
    {
        // notify exit wait queue
        list_head *it=NULL, *next=NULL;
        list_for_each_safe(it, next, &co->exit_notify_queue)
        {
            coroutine_t * co2 = container_of(it, coroutine_t, wait_to);
            list_del_init(it);
            resume(co2, (void *)(int64_t)(co->ret_val));
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

uint64_t g_coroutine_next_id=1;

int init_coroutine(coroutine_t * self)
{

    self->ret_val = 0;
    self->is_enable_sys_hook = 1;
    self->is_end_delete = 0;
    self->is_enable_pthread_hook = 1;
    self->is_enable_disk_io_hook = 1;
    self->is_main =0;
    self->desc = NULL;
    self->gc_mgr = NULL;
    self->static_vars = NULL;
    self->spec = NULL;
    self->pthread_spec = NULL;
    self->id = g_coroutine_next_id++;
    self->stack = NULL;
    self->stack_size = 0;
    INIT_LIST_HEAD(&self->wait_to);
    INIT_LIST_HEAD(&self->exit_notify_queue);
    self->state = CREATE;
    return 0;
}

__thread fixed_mempool_t * g_default_stack_pool=NULL;


fixed_mempool_t *get_default_stack_pool()
{
    if (NULL == g_default_stack_pool)
    {
        fixed_mempool_t *pool = new fixed_mempool_t();
        pool->init(FLAGS_stack_size, 10000, 64); // 64 bytes for cache_line align
        if (NULL == g_default_stack_pool)
        {
            g_default_stack_pool = pool;
        } else {
            delete g_default_stack_pool;
        }
    }
    return g_default_stack_pool;
}

#define CACHE_LINE_SIZE 64
int set_callback(coroutine_t * self, CO_MAIN_FUN * fn, void * arg, int stack_size)
{
    fixed_mempool_t * pool = get_default_stack_pool();
    if (stack_size == FLAGS_stack_size) {
        self->stack = pool->alloc();
        stack_size = pool->alloc_size;
        self->is_page_stack = pool->is_page_alloc;
    } else {
        // stack  group from high address to  low; align depend stack_size must be multiplies align size
        stack_size =  (stack_size+CACHE_LINE_SIZE-1)/CACHE_LINE_SIZE * CACHE_LINE_SIZE;
        if ((stack_size >= g_page_size) && (g_page_size > 0)) {
            stack_size = (stack_size + g_page_size -1) / g_page_size * g_page_size;
            self->stack = mmap(NULL, stack_size, PROT_READ| PROT_WRITE, MAP_PRIVATE | MAP_ANONYMOUS, -1, 0); 
            self->is_page_stack = 1;
        } else {
            self->stack = memalign(CACHE_LINE_SIZE, stack_size); 
            self->is_page_stack = 0;
        }
    }

    self->stack_size = stack_size;
    self->ctx.uc_stack.ss_sp = self->stack;
    self->ctx.uc_stack.ss_size = stack_size;
    self->ctx.uc_link = NULL;

    self->pfn = fn;
    self->pfn_arg = arg;

#ifdef USE_VALGRIND
    self->m_vid = VALGRIND_STACK_REGISTER(self->stack, (char *)self->stack + stack_size);
#endif

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

coroutine_t * alloc_coroutine(CO_MAIN_FUN * fn, void * arg,  uint32_t stack_size)
{
    //coroutine_t *co = ALLOC_VAR(coroutine_t);
    coroutine_t *co = (coroutine_t *)get_co_struct_pool()->alloc();
    if (stack_size <=0) {
        stack_size = FLAGS_stack_size;
    }
    init_coroutine(co);
    set_callback(co, fn, arg, stack_size);
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

    if (co->static_vars ) delete co->static_vars;

    if (co->spec) delete co->spec;

    if (co->pthread_spec) delete co->pthread_spec;


    #ifdef USE_VALGRIND
            VALGRIND_STACK_DEREGISTER(co->m_vid);
    #endif

    if (co->stack_size == FLAGS_stack_size) 
    {
        get_default_stack_pool()->free(co->stack);
    } 
    else
    {
        if (co->is_page_stack)
        {
            munmap(co->stack, co->stack_size);
        }
        else 
        {
            free(co->stack);
        }
    }
    co->stack = NULL;
    get_co_struct_pool()->free(co);
}


typedef void (*coroutine_fun_t)();

void *resume(coroutine_t * co, void * val)
{
    coroutine_env_t *env = get_coroutine_env();
    coroutine_t *cur = env->curr_co;
    assert(cur);
    co->yield_val = val;
    if (CREATE == co->state) {
        uint64_t p = (uint64_t) co;
        //getcontext(&co->ctx);
        makecontext(&co->ctx, (coroutine_fun_t) co_main_helper, 2, \
                    (uint32_t)(p & 0xffffffff), (uint32_t)((p >> 32) & 0xffffffff) );
    }
    co->ctx.uc_link = &cur->ctx;
    co->state = RUNNING;
    list_del_init(&co->wait_to);
    env->curr_co = co;
    list_add_tail(&cur->wait_to, &env->run_queue);

    conet_swapcontext(&(cur->ctx), &(co->ctx) );
    return cur->yield_val;
}

void * yield(list_head *wait_to, void * val)
{
    coroutine_env_t *env = get_coroutine_env();
    coroutine_t *cur = env->curr_co;

    if (list_empty(&env->run_queue)) {
        LOG(FATAL)<<"co thread env run queue empty";
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
    conet_swapcontext(&cur->ctx, &last->ctx);
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
    int ret = 0;
    if (co->state >= STOP) {
        return co->ret_val; 
    }

    ret = (int64_t)(yield(&co->exit_notify_queue, NULL));
    return ret;
}

void wait_timeout_handle(void *arg)
{
    coroutine_t * co = (coroutine_t *)(arg);
    list_del_init(&co->wait_to);
    resume(co, (void *)-1); 
}

int wait(coroutine_t *co, uint32_t ms)
{
    if (co->state >= STOP) {
        return co->ret_val; 
    }

    timeout_handle_t wait_th;
    init_timeout_handle(&wait_th, wait_timeout_handle, CO_SELF(), ms);
    set_timeout(&wait_th, ms);

    int ret = (int64_t) (yield(&co->exit_notify_queue, NULL));
    cancel_timeout(&wait_th);
    return ret;
}

class TimeoutHandle; 

class TimeoutMgr
{
public:
    std::map<uint64_t, TimeoutHandle *> m_tm;
    uint64_t m_id;
    TimeoutMgr()
    {
        m_id = 0;
    }

    uint64_t add(TimeoutHandle *t)
    {
        ++m_id;
        m_tm.insert(std::make_pair(m_id, t));
        return m_id;
    }
    int free(uint64_t id)
    {
        m_tm.erase(id);
        return 0;
    }
    TimeoutHandle * get(uint64_t id)
    {
        return m_tm.at(id);
    }
};




__thread class TimeoutMgr * g_timout_mgr = NULL;

CONET_DEF_TLS_VAR_HELP_DEF(g_timout_mgr);

class TimeoutHandle
{
public:

    TimeoutHandle(void (*fn)(void *), void *arg, uint64_t ms, int inter = 0, int stack_size=0)
    {
        m_arg = arg;
        m_fn = fn;
        m_ms = ms;
        m_inter = m_inter;
        m_exit_flag = 0;

        init_timeout_handle(&m_tm,  &TimeoutHandle::timeout_cb, this, m_ms);
        m_co = alloc_coroutine(&TimeoutHandle::proc, this, stack_size);
        set_auto_delete(m_co);
        if (inter) {
            set_interval(&m_tm, m_ms);
        } else {
            set_timeout(&m_tm, m_ms);
        }
        m_id = TLS_GET(g_timout_mgr)->add(this);
    }

    timeout_handle_t m_tm;
    
    void (*m_fn)(void *);
    void *m_arg; 
    uint64_t m_ms;
    uint64_t m_id;
    int m_exit_flag;
    int m_inter;

    coroutine_t *m_co;

    static
    void timeout_cb(void *arg)
    {
        TimeoutHandle *self = (TimeoutHandle *)(arg);
        conet::resume(self->m_co);
    }

    static
    int proc(void *arg)
    {
        TimeoutHandle *self = (TimeoutHandle *)(arg);
        while(!self->m_exit_flag) {
          (self->m_fn)(self->m_arg);
          if (0 == self->m_inter) break; 
          conet::yield(NULL, NULL);
        } 
        delete self;
        return 0;
    }
   
    ~TimeoutHandle()
    {
       TLS_GET(g_timout_mgr)->free(m_id);
    }
};

uint64_t set_timeout(void (*fn)(void *), void *arg, int ms, int stack_size)
{
    TimeoutHandle *tm = new TimeoutHandle(fn, arg, ms, 0, stack_size);
    return tm->m_id;
}

uint64_t set_interval(void (*fn)(void *), void *arg, int ms, int stack_size)
{
    TimeoutHandle *tm = new TimeoutHandle(fn, arg, ms, 1, stack_size);
    return tm->m_id;
}

void cancel_timeout(uint64_t id) 
{
   TimeoutHandle * tm = TLS_GET(g_timout_mgr)->get(id); 
   if (tm) {
       tm->m_exit_flag = 1;
       conet::resume(tm->m_co);
   }
}

void cancel_interval(uint64_t id) 
{
    return cancel_timeout(id);
}

template<typename T>
T call_closure(void *arg)
{
    Closure<T> * cl = (Closure<T> *)(arg);
    return cl->Run();
}


uint64_t set_timeout(Closure<void> *cl, int ms, int stack_size)
{
    return set_timeout(&call_closure<void>, cl, ms, stack_size);
}

uint64_t set_interval(Closure<void> *cl, int ms, int stack_size)
{
    return set_interval(&call_closure<void>, cl, ms, stack_size);
}

}

