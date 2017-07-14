#include <pthread.h>
#include <unistd.h>
#include <assert.h>
#include <sys/syscall.h>
#include <stdint.h>
#include <malloc.h>
#include <sys/mman.h>

#include "coroutine.h"
#include "coroutine_env.h"
#include "timewheel.h"
#include "gflags/gflags.h"
#include "log.h"
#include "../../base/fixed_mempool.h"
#include "../../base/tls.h"

DEFINE_int32(stack_size, 4*4096, "default stack size bytes");

extern "C" void conet_swapcontext(ucontext_t *co, ucontext_t *co2);
extern "C" void conet_setcontext(ucontext_t *co);


namespace conet
{


bool is_stop(coroutine_t *co)
{
    return co->state == STOP;
}

static
void co_return(coroutine_t *co)
{
    coroutine_env_t *env = co->env;
    if (list_empty(&env->run_queue)) {
        PLOG_FATAL("co thread env run queue empty");
        return ;
    }

    coroutine_t *last = container_of(env->run_queue.prev, coroutine_t, wait_to);
    list_del_init(&last->wait_to);

    env->curr_co = last;
    last->state = RUNNING;
    jump_fcontext(&(co->fctx), last->fctx, NULL);
}

int delay_del_coroutine(void *arg)
{
    coroutine_t * co = (coroutine_t *)(arg);
    free_coroutine(co);
    return 0;
}

int64_t g_page_size  = sysconf(_SC_PAGESIZE);

struct exit_notify_t
{
    list_head queue;
    int ret_val;
};

static 
int proc_exit_notify(void *arg)
{
    exit_notify_t * data = (exit_notify_t *)(arg);

    list_head *it=NULL, *next=NULL;
    list_for_each_safe(it, next, &data->queue)
    {
        list_del_init(it);
        coroutine_t * co2 = container_of(it, coroutine_t, wait_to);
        if (co2->state < STOP) 
        {
            resume(co2, (void *)(int64_t)(data->ret_val));
        }
    }
    delete data;
    return 0;
}

static
void co_main_helper2(void *p)
{
    coroutine_t *co = (coroutine_t *)p;

    //run main proc
    co->ret_val = co->pfn(co->pfn_arg);

    co->state = STOP;

    list_del_init(&co->wait_to);
    if (!list_empty(&co->exit_notify_queue))
    {
        exit_notify_t * data = new exit_notify_t();
        INIT_LIST_HEAD(&data->queue);
        list_add(&data->queue, &co->exit_notify_queue);
        list_del_init(&co->exit_notify_queue);
        data->ret_val = co->ret_val;
        registry_delay_task(proc_exit_notify, data);
    }

    if (co->is_end_delete) 
    {
        // 延迟删除
        coroutine_env_t *env = co->env;
        conet::task_t task;
        init_task(&task, delay_del_coroutine, co);
        list_add_tail(&task.link_to, &env->delay_del_list);
        env->dispatch->delay(&env->delay_del_task);
        co_return(co);
        return;
    }

    co_return(co);
    return ;
}

uint64_t __attribute__((aligned(8))) g_coroutine_next_id=1;

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
    self->id = __sync_fetch_and_add(&g_coroutine_next_id, 1);
    self->stack = NULL;
    self->stack_size = 0;
    INIT_LIST_HEAD(&self->wait_to);
    INIT_LIST_HEAD(&self->exit_notify_queue);
    self->state = CREATE;
    return 0;
}

void free_default_stack_pool(void *p)
{
    fixed_mempool_t *p1 = (fixed_mempool_t *)(p);
    delete p1;
}

#define CACHE_LINE_SIZE 64LL
int set_callback(coroutine_t * self, CO_MAIN_FUN * fn, void * arg, int stack_size)
{
    fixed_mempool_t * pool = &self->env->default_stack_pool;
    if (stack_size == FLAGS_stack_size) {
        self->stack = pool->alloc();
        stack_size = pool->alloc_size;
        self->is_page_stack = pool->is_page_alloc;
    } else {
        // stack  group from high address to  low; align depend stack_size must be multiplies align size
        stack_size =  stack_size & -CACHE_LINE_SIZE;
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
    coroutine_env_t *env = get_coroutine_env();
    coroutine_t *co = (coroutine_t *)env->co_struct_pool.alloc();
    if (stack_size <=0) {
        stack_size = FLAGS_stack_size;
    }
    init_coroutine(co);
    co->env = env;
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

    if (co->static_vars ) 
    {
        // 静态变量， 从 gc_mgr 分配的, 不需要回收
        delete co->static_vars;
    }

    if (co->spec) 
    {
        // 从 gc_mgr 分配, 没办法回收
        delete co->spec;
    }

    if (co->pthread_spec) 
    {
        // 没办法回收
        delete co->pthread_spec;
    }


    #ifdef USE_VALGRIND
            VALGRIND_STACK_DEREGISTER(co->m_vid);
    #endif

    if (co->stack_size == (uint64_t)FLAGS_stack_size) 
    {
        co->env->default_stack_pool.free(co->stack);
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
    co->env->co_struct_pool.free(co);
}


typedef void (*coroutine_fun_t)();

void *resume(coroutine_env_t * env, coroutine_t * co, void * val)
{
    coroutine_t *cur = env->curr_co;
    env->curr_co = co;

    if (unlikely(CREATE == co->state)) 
    {
        co->fctx = make_fcontext((char *)co->stack + co->stack_size, co->stack_size, co_main_helper2);
        val = co;
    }

    co->state = RUNNING;
    if (unlikely(!list_empty(&co->wait_to))) 
    {
        list_del_init(&co->wait_to);
    }

    list_del_init(&cur->wait_to);
    list_add_tail(&cur->wait_to, &env->run_queue);

    return  jump_fcontext(&(cur->fctx), co->fctx, val);
}

void *resume(coroutine_t * co, void * val)
{

    coroutine_env_t *env = get_coroutine_env();
    return resume(env, co, val);
}

void * yield(list_head *wait_to, void * val)
{
    coroutine_env_t *env = get_coroutine_env();
    coroutine_t *cur = env->curr_co;

    if (list_empty(&env->run_queue)) {
       PLOG_FATAL("cur run queue is empty!");
       abort();
    }

    coroutine_t *last = container_of(list_pop_tail(&env->run_queue), coroutine_t, wait_to);

    env->curr_co = last;

    if (unlikely(wait_to)) 
    {
        list_del_init(&cur->wait_to);
        list_add_tail(&cur->wait_to, wait_to);
    } 

    cur->state = SUSPEND;
    last->state = RUNNING;
    return  jump_fcontext(&(cur->fctx), last->fctx, val);
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

    /*
    coroutine_env_t *env = get_coroutine_env();
    coroutine_t *cur = env->curr_co;
    if (unlikely(list_empty(&env->run_queue)))
    {
        list_del_init(&cur->wait_to);
        list_add_tail(&cur->wait_to, &co->exit_notify_queue);
        resume(co, NULL);
    } else {
    */
        ret = (int64_t)(yield(&co->exit_notify_queue, NULL));
    //}
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
    init_timeout_handle(&wait_th, wait_timeout_handle, CO_SELF());
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

        init_timeout_handle(&m_tm,  &TimeoutHandle::timeout_cb, this);
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


uint64_t set_timeout(closure_t<void> *cl, int ms, int stack_size)
{
    return set_timeout(&call_closure<void>, cl, ms, stack_size);
}

uint64_t set_interval(closure_t<void> *cl, int ms, int stack_size)
{
    return set_interval(&call_closure<void>, cl, ms, stack_size);
}

}

