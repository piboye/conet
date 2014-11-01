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
#include <sys/syscall.h>
#include <dlfcn.h>

#include "coroutine.h"
#include "coroutine_impl.h"
#include "timewheel.h"
#include "dispatch.h"

#include "base/incl/tls.h"
#include "base/incl/auto_var.h"
#include "base/incl/addr_map.h"
#include "pthread_hook.h"


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


using namespace conet;

struct pcond_mgr_t;

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


struct pcond_ctx_t
{
    list_head wait_item;
    conet::coroutine_t *co;
    list_head *schedule_queue;
    pthread_mutex_t *schedule_mutex;
    timeout_handle_t tm;
    int ret_timeout;
    pcond_mgr_t *mgr;
};

}

static int trylock(lock_ctx_t *ctx);

namespace conet 
{

struct pthread_mgr_t
{
    list_head lock_schedule_queue;   // lock schdule queue;
    list_head pcond_schedule_queue;  // pthread condition var notify schedule queue
    pthread_mutex_t pcond_schedule_mutex;
    conet::task_t schedule_task;     // 调度任务

    pthread_mgr_t()
    {
        INIT_LIST_HEAD(&lock_schedule_queue);
        INIT_LIST_HEAD(&pcond_schedule_queue);
        pthread_mutex_init(&pcond_schedule_mutex, NULL);
        AUTO_VAR(fn, =, &pthread_mgr_t::proc_pthread_schedule);
        task_proc_func_t p =  NULL;
        memcpy(&p, &fn, sizeof(void *)); // i hate c++ !!!!
        conet::init_task(&schedule_task, p, this);
    }

    ~pthread_mgr_t()
    {
        list_del(&lock_schedule_queue);
        list_del(&pcond_schedule_queue);
        pthread_mutex_unlock(&pcond_schedule_mutex);
    }
    int proc_pthread_schedule();

};

static __thread pthread_mgr_t *g_pthread_mgr = NULL;
CONET_DEF_TLS_VAR_HELP(g_pthread_mgr,
        ({
            pthread_mgr_t * m = new pthread_mgr_t();
            conet::registry_task(&m->schedule_task);
            m;
        }),
        ({
            delete self; 
        })
); 

}

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
            abort();
         return -1;
    }
    return -2;
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
    pthread_mgr_t *mgr = TLS_GET(g_pthread_mgr);
    lock_ctx_t ctx;
    INIT_LIST_HEAD(&ctx.wait_item);
    ctx.co = conet::current_coroutine();
    ctx.lock = mutex;
    ctx.type = MUTEX_TYPE;

    list_add_tail(&ctx.wait_item, &mgr->lock_schedule_queue);
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
    pthread_mgr_t *mgr = TLS_GET(g_pthread_mgr);
    lock_ctx_t ctx;
    INIT_LIST_HEAD(&ctx.wait_item);
    ctx.co = conet::current_coroutine();
    ctx.lock = rwlock;
    ctx.type = RDLOCK_TYPE; // read

    list_add_tail(&ctx.wait_item, &mgr->lock_schedule_queue);

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

    // add to wrlock schedule queue
    pthread_mgr_t *mgr = TLS_GET(g_pthread_mgr);
    lock_ctx_t ctx;
    INIT_LIST_HEAD(&ctx.wait_item);
    ctx.co = conet::current_coroutine();
    ctx.lock = rwlock;
    ctx.type = WRLOCK_TYPE;

    list_add_tail(&ctx.wait_item, &mgr->lock_schedule_queue);
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

    // add to spinlock schedule queue
    pthread_mgr_t *mgr = TLS_GET(g_pthread_mgr);
    lock_ctx_t ctx;
    INIT_LIST_HEAD(&ctx.wait_item);
    ctx.co = conet::current_coroutine();
    ctx.lock = (void *) lock;
    ctx.type = SPINLOCK_TYPE;

    list_add_tail(&ctx.wait_item, &mgr->lock_schedule_queue);

    conet::yield();
    return 0;
}



namespace {
class scope_lock
{
public:
    pthread_mutex_t *mutex;
    int cnt;
    explicit
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

class scope_rdlock
{
public:
    pthread_rwlock_t *lock;
    int cnt;
    explicit
    scope_rdlock(pthread_rwlock_t *l) 
    {
        lock = l;
        cnt = 0;
        _(pthread_rwlock_rdlock)(l);

    }
    ~scope_rdlock() {
        pthread_rwlock_unlock(lock);
    }
};

class scope_wrlock
{
public:
    pthread_rwlock_t *lock;
    int cnt;
    explicit
    scope_wrlock(pthread_rwlock_t *l) 
    {
        lock = l;
        cnt = 0;
        _(pthread_rwlock_wrlock)(l);
    }
    ~scope_wrlock() {
        pthread_rwlock_unlock(lock);
    }
};

}

#define SCOPE_RDLOCK(l) \
    for (scope_rdlock scope_rdlock_##__LINE__ (l); scope_rdlock_##__LINE__.cnt <=0; scope_rdlock_##__LINE__.cnt=1)


#define SCOPE_WRLOCK(l) \
    for (scope_wrlock scope_wrlock_##__LINE__ (l); scope_wrlock_##__LINE__.cnt <=0; scope_wrlock_##__LINE__.cnt=1)


#define SCOPE_LOCK(mutex) \
    for (scope_lock scope_lock_##__LINE__ (mutex); scope_lock_##__LINE__.cnt <=0; scope_lock_##__LINE__.cnt=1)

namespace conet 
{
    int pthread_mgr_t::proc_pthread_schedule()
    {
        int cnt =0;
        list_head * queue = &this->lock_schedule_queue;
        if (!list_empty(queue))
        { // lock schdule
            lock_ctx_t *ctx = NULL, *next= NULL;
            list_for_each_entry_safe(ctx, next, queue, wait_item)
            {
                int ret = trylock(ctx);
                if (ret == 0)  {
                    list_del_init(&ctx->wait_item);
                    conet::resume(ctx->co); 
                    ++cnt;
                }
            }
        }
        queue = &this->pcond_schedule_queue;
        if (!list_empty(queue))
        { // pthread condition var schedule
            LIST_HEAD(notify_queue);
            
            SCOPE_LOCK(&pcond_schedule_mutex)
            {
                list_add_tail(&pcond_schedule_queue, &notify_queue);
                list_del_init(&pcond_schedule_queue);
            }

            pcond_ctx_t * ctx = NULL, *next = NULL;
            list_for_each_entry_safe(ctx, next, &notify_queue, wait_item)
            {
                list_del_init(&ctx->wait_item);
                cancel_timeout(&ctx->tm);
                conet::resume(ctx->co); 
                ++cnt;
            }
        }

        return cnt;
    }
}

struct pcond_mgr_t
{
   conet::AddrMap::Node node;
   list_head wait_list;
   pthread_mutex_t mutex;

   explicit 
   pcond_mgr_t(void *key)
   {
        INIT_LIST_HEAD(&this->wait_list);
        this->node.init(key);
        pthread_mutex_init(&mutex, NULL);
   }

   void dtor()
   {
        SCOPE_LOCK(&this->mutex)
        {
            list_del_init(&this->wait_list);
        }
        pthread_mutex_destroy(&this->mutex);
   }

   static int fini(void *arg, conet::AddrMap::Node *n)
   {
        pcond_mgr_t *p = container_of(n, pcond_mgr_t, node);
        p->dtor();
        delete p;
        return 0;
   }
};



static pthread_rwlock_t g_cond_map_lock = PTHREAD_RWLOCK_INITIALIZER;

static conet::AddrMap * volatile g_cond_map = NULL;

static void delete_g_cond_map(int status, void *arg)
{
    conet::AddrMap * map = (conet::AddrMap *)(arg);
    delete map; 
}

inline
static conet::AddrMap * get_cond_map()  
{
    conet::AddrMap * g_map = g_cond_map; // x86_64 8 byte read is atomic
    if (g_map == NULL) {
        conet::AddrMap *map = new conet::AddrMap();
        map->init(1000);
        map->set_destructor_func(&pcond_mgr_t::fini, NULL);
        if (__sync_bool_compare_and_swap(&g_cond_map, NULL, map)) {
            on_exit(delete_g_cond_map, map);
            return map;
        } else {
            delete_g_cond_map(0, map);
            return g_cond_map;
        }
    } else {
        return g_map;
    }
}


static pcond_mgr_t * find_cond_map_can_null(void *key)
{
    conet::AddrMap *map = get_cond_map();

    SCOPE_RDLOCK(&g_cond_map_lock);
    {
        conet::AddrMap::Node * node = map->find(key);
        if (node) {
            return container_of(node, pcond_mgr_t, node);
        } else {
            return NULL;
        }
    }
}

static pcond_mgr_t * find_cond_map(void *key)
{
    conet::AddrMap *map = get_cond_map();

    conet::AddrMap::Node * node = NULL;
    SCOPE_RDLOCK(&g_cond_map_lock);
    { 
        node = map->find(key);
        if (node) {
            return container_of(node, pcond_mgr_t, node);
        } 
    }

    pcond_mgr_t * mgr = new pcond_mgr_t(key);
    pcond_mgr_t * old_mgr = NULL;
    SCOPE_WRLOCK(&g_cond_map_lock)
    {
        node = map->find(key);
        if (node) {
            old_mgr =  container_of(node, pcond_mgr_t, node);
        } else {
            map->add(&mgr->node);
            return mgr;
        }
    }
    delete mgr;
    return old_mgr;
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

    pthread_mgr_t *pmgr = TLS_GET(g_pthread_mgr);

    pcond_ctx_t ctx;
    ctx.ret_timeout = 0;
    init_timeout_handle(&ctx.tm, NULL, NULL);
    ctx.co = conet::current_coroutine();

    INIT_LIST_HEAD(&ctx.wait_item);

    ctx.schedule_queue = &pmgr->pcond_schedule_queue;
    ctx.schedule_mutex = &pmgr->pcond_schedule_mutex;

    pcond_mgr_t *mgr = find_cond_map(cond);
    ctx.mgr = mgr;
    SCOPE_LOCK(&mgr->mutex) 
    {
        list_add_tail(&ctx.wait_item, &mgr->wait_list);
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

    SCOPE_LOCK(&ctx->mgr->mutex)
    {
        list_del_init(&ctx->wait_item);
    }

    SCOPE_LOCK(ctx->schedule_mutex) 
    {
        list_add_tail(&ctx->wait_item, ctx->schedule_queue);
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

    pcond_ctx_t ctx;
    ctx.ret_timeout = 0;
    init_timeout_handle(&ctx.tm, proc_pthread_cond_timeout, &ctx);
    if(abstime) 
    {   
        uint64_t timeout = (abstime->tv_sec*1000 + abstime->tv_nsec/1000000);
        uint64_t now = conet::get_sys_ms();  // 必须是系统的时间
        if (timeout < now) timeout = 0;
        timeout -= now;
        set_timeout(&ctx.tm, timeout);
    }    

    pthread_mgr_t *pmgr = TLS_GET(g_pthread_mgr);

    ctx.co = conet::current_coroutine();
    INIT_LIST_HEAD(&ctx.wait_item);
    ctx.schedule_queue = &pmgr->pcond_schedule_queue;
    ctx.schedule_mutex = &pmgr->pcond_schedule_mutex;

    pcond_mgr_t *mgr = find_cond_map(cond);
    ctx.mgr = mgr;
    SCOPE_LOCK(&mgr->mutex)
    {
        list_add_tail(&ctx.wait_item, &mgr->wait_list);
    }

    if (mutex) pthread_mutex_unlock(mutex);
    conet::yield();
    if (mutex) _(pthread_mutex_lock)(mutex);
    if (ctx.ret_timeout) {
        return ETIMEDOUT;
    }
    return 0;
}



static 
inline
int pthread_cond_signal_help(pthread_cond_t *cond, bool only_one)
{
    int cnt = 0;
    pcond_mgr_t *mgr = NULL;
    mgr= find_cond_map_can_null(cond);
    if (mgr) { 
        if (list_empty(&mgr->wait_list)) return 0;
        LIST_HEAD(notify_queue);
        pcond_ctx_t *ctx=NULL, *next=NULL;
        SCOPE_LOCK(&mgr->mutex)
        {
            if (list_empty(&mgr->wait_list)) return 0;
            if (only_one) {
                list_add_tail(list_pop_head(&mgr->wait_list), &notify_queue);
            } else {
                list_add_tail(&mgr->wait_list, &notify_queue);
                list_del_init(&mgr->wait_list);
            }
        }
        list_for_each_entry_safe(ctx, next, &notify_queue, wait_item)
        {
            ++cnt;
            SCOPE_LOCK(ctx->schedule_mutex) 
            {
                list_move_tail(&ctx->wait_item, ctx->schedule_queue);
            }
        }
    }
    return cnt;
}

HOOK_FUNC_DEF(int, pthread_cond_signal, (pthread_cond_t *cond))
{
	HOOK_FUNC(pthread_cond_signal);
    int cnt = pthread_cond_signal_help(cond, true);
    if (cnt  == 0) {
        return _(pthread_cond_signal)(cond);
    }
    return 0;
}

HOOK_FUNC_DEF(int, pthread_cond_broadcast,(pthread_cond_t *cond))
{

	HOOK_FUNC(pthread_cond_broadcast);
    pthread_cond_signal_help(cond, false); // 通知所有
    return _(pthread_cond_broadcast)(cond);
}

HOOK_FUNC_DEF(int, pthread_cond_destroy, (pthread_cond_t *cond))
{
	HOOK_FUNC(pthread_cond_destroy);
    pcond_mgr_t *mgr = find_cond_map_can_null(cond);
    if (mgr) {
        SCOPE_WRLOCK(&g_cond_map_lock)
        {   
            get_cond_map()->remove(&mgr->node);
        }
        mgr->dtor();
        delete mgr;
    }
    return _(pthread_cond_destroy)(cond);
}
