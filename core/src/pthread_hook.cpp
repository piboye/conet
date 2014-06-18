/*
 * =====================================================================================
 *
 *       Filename:  hook_pthread.cpp
 *
 *    Description:
 *
 *        Version:  1.0
 *        Created:  2014年05月20日 16时06分13秒
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
#include <errno.h>
#include "coroutine.h"
#include "coroutine_impl.h"
#include <sys/syscall.h>
#include <dlfcn.h>
#include "timewheel.h"
#include "dispatch.h"
#include "tls.h"

#define SYS_FUNC(name) g_sys_##name##_func
#define _(name) SYS_FUNC(name)

#define HOOK_FUNC_DEF(ret_type, name, proto) \
    typedef ret_type (* name##_pfn_t) proto; \
    name##_pfn_t _(name) = (name##_pfn_t) dlsym(RTLD_NEXT, #name) ; \
    ret_type name proto __THROW \
 

#define HOOK_FUNC(name) \
    do { \
        if( !_(name)) {  \
            _(name) = (name##_pfn_t) dlsym(RTLD_NEXT,#name);  \
        } \
    } while(0) \


namespace conet
{
int is_enable_pthread_hook()
{
    return current_coroutine()->is_enable_pthread_hook;
}

void enable_pthread_hook()
{
    current_coroutine()->is_enable_pthread_hook = 1;
}

void disable_pthread_hook()
{
    current_coroutine()->is_enable_pthread_hook = 0;
}

}

HOOK_FUNC_DEF(
    void*, pthread_getspecific, (pthread_key_t key)
)
{
    HOOK_FUNC(pthread_getspecific);
    if (!conet::is_enable_pthread_hook()) {
        return _(pthread_getspecific)(key);
    }

    return conet::get_pthread_spec(key);
}

HOOK_FUNC_DEF(
    int, pthread_setspecific, (pthread_key_t key, __const void *value)
)
{
    HOOK_FUNC(pthread_setspecific);
    if (!conet::is_enable_pthread_hook()) {
        return _(pthread_setspecific)(key, value);
    }

    return conet::set_pthread_spec(key, value);
}



static
void tls_fin_mutex(void *arg)
{
   pthread_mutex_t *mutex = (pthread_mutex_t *)(arg);
   pthread_mutex_destroy(mutex); 
   free(mutex);
}

static
pthread_mutex_t *
    tls_get_mutex( pthread_mutex_t * &mutex)
{
    if (NULL == mutex) {
        mutex = (pthread_mutex_t *)malloc(sizeof(pthread_mutex_t));
        pthread_mutex_init(mutex, NULL);
        tls_onexit_add(mutex, &tls_fin_mutex);
    }
    return mutex;
}

static
list_head *tls_get_list_head(list_head * &h)
{
    if (NULL == h) {
        h = new list_head();
        INIT_LIST_HEAD(h);
        tls_onexit_add(h, tls_destructor_fun<list_head>);
    }
    return h;
}


namespace {
enum {
    MUTEX_TYPE = 1,
    RDLOCK_TYPE = 2,
    WRLOCK_TYPE = 3,
    SPINLOCK_TYPE = 4,
};

struct lock_ctx_t
{
    list_head wait_item;
    conet::coroutine_t *co;
    void *lock;
    int type; // 1 mutex; 2 rdlock; 3 wdlock; 4 spinlock
};
}

static __thread list_head * g_lock_schedule_queue = NULL;
static
list_head *get_lock_schedule_queue();

static conet::task_t g_lock_dipatch_task;

static int trylock(lock_ctx_t *ctx) 
{
    switch(ctx->type)
    {
        case MUTEX_TYPE:
            {
                pthread_mutex_t * mutex =  (pthread_mutex_t *)(ctx->lock);
                return pthread_mutex_trylock(mutex);
            }
        case RDLOCK_TYPE:
            {
                pthread_rwlock_t * lock =  (pthread_rwlock_t *)(ctx->lock);
                return pthread_rwlock_tryrdlock(lock);
            }
        case WRLOCK_TYPE:
            {
                pthread_rwlock_t * lock =  (pthread_rwlock_t *)(ctx->lock);
                return pthread_rwlock_trywrlock(lock);
            }
        case SPINLOCK_TYPE:
            {
                pthread_spinlock_t * lock =  (pthread_spinlock_t *)(ctx->lock);
                return pthread_spin_trylock(lock);
            }
        default:
            fprintf(stderr, "error lock type:%d\n", ctx->type);
            exit(-1);
            return 0;

    }
    return 0;
}

static
int proc_lock_schedule(void *arg)
{
    list_head *list = (list_head *)(arg);
    int cnt =0;
    list_head *it=NULL, *next=NULL;
    list_for_each_safe(it, next, list)
    {
        lock_ctx_t * ctx = container_of(it, lock_ctx_t, wait_item);
        int ret = trylock(ctx);
        if (ret == 0)  {
            list_del_init(it);
            conet::resume(ctx->co); 
            ++cnt;
        }
    }
    if (list_empty(list)) {
       unregistry_task(&g_lock_dipatch_task);
    }
    return cnt;
}

static
list_head *get_lock_schedule_queue() 
{
    if (NULL == g_lock_schedule_queue) {
        g_lock_schedule_queue = new list_head();
        INIT_LIST_HEAD(g_lock_schedule_queue);
        tls_onexit_add(g_lock_schedule_queue, tls_destructor_fun<list_head>);
        conet::init_task(&g_lock_dipatch_task, 
                proc_lock_schedule, g_lock_schedule_queue);
    }
    if (list_empty(g_lock_schedule_queue)) {
        conet::registry_task(&g_lock_dipatch_task);
    }
    return g_lock_schedule_queue;
}



HOOK_FUNC_DEF(int, pthread_mutex_lock,(pthread_mutex_t *mutex))
{
	HOOK_FUNC(pthread_mutex_lock);
	if(!conet::is_enable_pthread_hook())
	{
        return _(pthread_mutex_lock)(mutex);
    }

    int ret = 0;
    ret = pthread_mutex_trylock(mutex);
    if (0 == ret) return ret;

    // add to mutex schedule queue
    lock_ctx_t ctx;
    INIT_LIST_HEAD(&ctx.wait_item);
    ctx.co = conet::current_coroutine();
    ctx.lock = mutex;
    ctx.type = MUTEX_TYPE;

    list_add_tail(&ctx.wait_item, get_lock_schedule_queue());

    conet::yield();
    return 0;
}

HOOK_FUNC_DEF(int, pthread_rwlock_rdlock,(pthread_rwlock_t *rwlock))
{
    HOOK_FUNC(pthread_rwlock_rdlock);
	if(!conet::is_enable_pthread_hook())
	{
        return _(pthread_rwlock_rdlock)(rwlock);
    }
    int ret = 0;
    ret = pthread_rwlock_tryrdlock(rwlock);
    if (0 == ret) return ret;

    // add to rdlock schedule queue
    lock_ctx_t ctx;
    INIT_LIST_HEAD(&ctx.wait_item);
    ctx.co = conet::current_coroutine();
    ctx.lock = rwlock;
    ctx.type = RDLOCK_TYPE; // read

    list_add_tail(&ctx.wait_item, get_lock_schedule_queue());

    conet::yield();
    return 0;
}

HOOK_FUNC_DEF(int, pthread_rwlock_wrlock,(pthread_rwlock_t *rwlock))
{
    HOOK_FUNC(pthread_rwlock_wrlock);
	if(!conet::is_enable_pthread_hook())
	{
        return _(pthread_rwlock_wrlock)(rwlock);
    }
    int ret = 0;
    ret = pthread_rwlock_trywrlock(rwlock);
    if (0 == ret) return ret;

    // add to rdlock schedule queue
    lock_ctx_t ctx;
    INIT_LIST_HEAD(&ctx.wait_item);
    ctx.co = conet::current_coroutine();
    ctx.lock = rwlock;
    ctx.type = WRLOCK_TYPE;

    list_add_tail(&ctx.wait_item, get_lock_schedule_queue());

    conet::yield();
    return 0;
}

HOOK_FUNC_DEF(int, pthread_spin_lock,(pthread_spinlock_t *lock))
{
    HOOK_FUNC(pthread_spin_lock);
	if(!conet::is_enable_pthread_hook())
	{
        return _(pthread_spin_lock)(lock);
    }
    int ret = 0;
    ret = pthread_spin_trylock(lock);
    if (0 == ret) return ret;

    // add to rdlock schedule queue
    lock_ctx_t ctx;
    INIT_LIST_HEAD(&ctx.wait_item);
    ctx.co = conet::current_coroutine();
    ctx.lock = (void *) lock;
    ctx.type = SPINLOCK_TYPE;

    list_add_tail(&ctx.wait_item, get_lock_schedule_queue());

    conet::yield();
    return 0;
}

namespace {
class scope_lock
{
public:
    pthread_mutex_t *mutex;
    int cnt;
    scope_lock(pthread_mutex_t *m) 
    {
        mutex = m;
        cnt = 0;
        _(pthread_mutex_lock)(m);

    }
    ~scope_lock() {
        pthread_mutex_unlock(mutex);
    }
};
}

#define SCOPE_LOCK(mutex) \
    for (scope_lock scope_lock_##__LINE__(mutex); scope_lock_##__LINE__.cnt <=0; scope_lock_##__LINE__.cnt=1)

static __thread list_head * g_pcond_schedule_queue =NULL;
static __thread pthread_mutex_t* g_pcond_schedule_mutex =NULL;

static pthread_mutex_t g_cond_mgr_mutex = PTHREAD_MUTEX_INITIALIZER;

struct pcond_ctx_t
{
    list_head wait_item;
    conet::coroutine_t *co;
    list_head *schedule_queue;
    pthread_mutex_t *schedule_mutex;
    timeout_handle_t tm;
    int ret_timeout;
};

struct pcond_mgr_t
{
   list_head wait_list;
};

static
int proc_pcond_schule(void *arg)
{
    int cnt = 0;
    list_head *it=NULL, *next=NULL;
    _(pthread_mutex_lock)(tls_get_mutex(g_pcond_schedule_mutex));
    list_for_each_safe(it, next, g_pcond_schedule_queue)
    {
        pcond_ctx_t * ctx = container_of(it, pcond_ctx_t, wait_item);
        list_del_init(it);
        cancel_timeout(&ctx->tm);
        pthread_mutex_unlock(tls_get_mutex(g_pcond_schedule_mutex));
        conet::resume(ctx->co); 
        ++cnt;
        _(pthread_mutex_lock)(tls_get_mutex(g_pcond_schedule_mutex));
    }
    pthread_mutex_unlock(tls_get_mutex(g_pcond_schedule_mutex));
    return cnt;
}

static std::map<void *, pcond_mgr_t*> g_cond_map;


static
list_head *get_pcond_schedule_list() {
    if (NULL == g_pcond_schedule_queue) {
        g_pcond_schedule_queue = new list_head();
        INIT_LIST_HEAD(g_pcond_schedule_queue);
        conet::registry_task(proc_pcond_schule, g_pcond_schedule_queue); 
        tls_onexit_add(g_pcond_schedule_queue, tls_destructor_fun<list_head>);
    }
    return g_pcond_schedule_queue;
}


HOOK_FUNC_DEF(int, pthread_cond_wait,
        (pthread_cond_t * __restrict cond, 
            pthread_mutex_t * __restrict mutex
            )) 
{
	HOOK_FUNC(pthread_cond_wait);
	if(!conet::is_enable_pthread_hook())
	{
        return _(pthread_cond_wait)(cond, mutex);
    }

    pcond_ctx_t wait_item;
    wait_item.ret_timeout = 0;
    init_timeout_handle(&wait_item.tm, NULL, NULL);
    wait_item.co = conet::current_coroutine();

    INIT_LIST_HEAD(&wait_item.wait_item);

    wait_item.schedule_queue = get_pcond_schedule_list();
    wait_item.schedule_mutex = tls_get_mutex(g_pcond_schedule_mutex);

    SCOPE_LOCK(&g_cond_mgr_mutex) 
    {
        pcond_mgr_t *mgr = g_cond_map[cond];
        if (NULL == mgr) {
            mgr = new pcond_mgr_t();
            INIT_LIST_HEAD(&mgr->wait_list);
            g_cond_map[cond] = mgr;
        }

        list_add_tail(&wait_item.wait_item, &mgr->wait_list);
    }

    if (mutex) pthread_mutex_unlock(mutex);
    conet::yield();
    if (mutex) _(pthread_mutex_lock)(mutex);

    return 0;
}


static
void proc_pthread_cond_timeout(void *arg)
{
    pcond_ctx_t *ctx = (pcond_ctx_t *) (arg);
    ctx->ret_timeout = 1;
    SCOPE_LOCK(&g_cond_mgr_mutex)
    {
        list_del_init(&ctx->wait_item);
        SCOPE_LOCK(ctx->schedule_mutex) 
        {
            list_add_tail(&ctx->wait_item, ctx->schedule_queue);
        }
    }
}

HOOK_FUNC_DEF(int, pthread_cond_timedwait,
        (pthread_cond_t *__restrict  cond, 
            pthread_mutex_t * __restrict mutex, 
            const struct timespec * __restrict abstime
            )) 
{
	HOOK_FUNC(pthread_cond_timedwait);
	if(!conet::is_enable_pthread_hook())
	{
        return _(pthread_cond_timedwait)(cond, mutex, abstime);
    }

    // coroutine

    pcond_ctx_t wait_item;
    wait_item.ret_timeout = 0;
    init_timeout_handle(&wait_item.tm, proc_pthread_cond_timeout, &wait_item);
    if(abstime) 
    {   
        uint64_t timeout = (abstime->tv_sec*1000 + abstime->tv_nsec/1000000);
        uint64_t now = get_sys_ms(); 
        if (timeout < now) timeout = 0;
        timeout -= now;
        set_timeout(&wait_item.tm, timeout);
    }    

    wait_item.co = conet::current_coroutine();
    INIT_LIST_HEAD(&wait_item.wait_item);
    wait_item.schedule_queue = get_pcond_schedule_list();
    wait_item.schedule_mutex = tls_get_mutex(g_pcond_schedule_mutex);

    SCOPE_LOCK(&g_cond_mgr_mutex)
    {
        pcond_mgr_t *mgr = g_cond_map[cond];
        if (NULL == mgr) {
            mgr = new pcond_mgr_t();
            INIT_LIST_HEAD(&mgr->wait_list);
            g_cond_map[cond] = mgr;
        }

        list_add_tail(&wait_item.wait_item, &mgr->wait_list);
    }

    if (mutex) pthread_mutex_unlock(mutex);
    conet::yield();
    if (mutex) _(pthread_mutex_lock)(mutex);
    if (wait_item.ret_timeout) {
        return ETIMEDOUT;
    }
    return 0;
}

HOOK_FUNC_DEF(int, pthread_cond_signal, (pthread_cond_t *cond))
{
	HOOK_FUNC(pthread_cond_signal);
    int cnt = 0;
    pcond_mgr_t *mgr = NULL;
    SCOPE_LOCK(&g_cond_mgr_mutex)
    {
        mgr= g_cond_map[cond];
        if (NULL == mgr) {
            mgr = new pcond_mgr_t();
            INIT_LIST_HEAD(&mgr->wait_list);
            g_cond_map[cond] = mgr;
        }

        list_head *it=NULL, *next=NULL;
        list_for_each_safe(it, next, &mgr->wait_list)
        {
            ++cnt;
            pcond_ctx_t * ctx = container_of(it, pcond_ctx_t, wait_item);

            list_del_init(it);

            SCOPE_LOCK(ctx->schedule_mutex) 
            {
                list_add_tail(it, ctx->schedule_queue);
            }
            break; 
        }
    }
    if (cnt  == 0) {
        return _(pthread_cond_signal)(cond);
    }
    return 0;
}

HOOK_FUNC_DEF(int, pthread_cond_broadcast,(pthread_cond_t *cond))
{

	HOOK_FUNC(pthread_cond_broadcast);
    pcond_mgr_t *mgr = NULL;
    SCOPE_LOCK(&g_cond_mgr_mutex)
    {   
        mgr = g_cond_map[cond];
        if (NULL == mgr) {
            mgr = new pcond_mgr_t();
            INIT_LIST_HEAD(&mgr->wait_list);
            g_cond_map[cond] = mgr;
        }

        list_head *it=NULL, *next=NULL;
        list_for_each_safe(it, next, &mgr->wait_list)
        {
            pcond_ctx_t * ctx = container_of(it, pcond_ctx_t, wait_item);
            list_del_init(it);
            SCOPE_LOCK(ctx->schedule_mutex) {
                list_add_tail(it, ctx->schedule_queue);
            }
        }
    }
    return _(pthread_cond_broadcast)(cond);
}

HOOK_FUNC_DEF(int, pthread_cond_destroy, (pthread_cond_t *cond))
{
	HOOK_FUNC(pthread_cond_destroy);
    return _(pthread_cond_destroy)(cond);
}
