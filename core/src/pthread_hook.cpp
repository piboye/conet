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
#include "base/auto_var.h"
#include "base/addr_map.h"
#include "event_notify.h"
#include "base/llist.h"

#include "coroutine.h"
#include "coroutine_env.h"
#include "timewheel.h"
#include "dispatch.h"
#include "event_notify.h"
#include "pthread_hook.h"

#include "base/time_helper.h"
#include "base/auto_var.h"
#include "base/addr_map.h"
#include "base/scope_lock.h"
#include "hook_helper.h"

using namespace conet;
namespace conet 
{

struct pcond_mgr_t;
struct pcond_ctx_t;
struct pthread_mgr_t
{
    // condition var  调度队列
    llist_head pcond_schedule_queue; 
    event_notify_t pcond_event_notify;
    coroutine_env_t * co_env;

    explicit pthread_mgr_t(coroutine_env_t *);
    ~pthread_mgr_t();

    static int pcond_event_cb(void *arg, uint64_t num);
    void add_to_pcond_schedule(pcond_ctx_t *ctx);
};

void free_pthread_mgr(pthread_mgr_t* self)
{
    delete self;
}

struct pcond_ctx_t
{
    list_head wait_item;
    conet::coroutine_t *co;
    timeout_handle_t tm;
    int ret_timeout;

    pcond_mgr_t *mgr;
    pthread_mgr_t *pmgr;

    // 用于 pcond_schedule_queue 
    llist_node link_to_schedule;

    pcond_ctx_t()
    {
        INIT_LIST_HEAD(&wait_item);
        ret_timeout = 0;
        //init_timeout_handle(&tm, NULL, NULL);
        mgr= NULL;
        pmgr = NULL;
        co = NULL;
    }
};

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

pthread_mgr_t * get_pthread_mgr() {
    coroutine_env_t *env = get_coroutine_env();
    if (NULL == env->pthread_mgr) {
        env->pthread_mgr = new pthread_mgr_t(env);
    }
    return env->pthread_mgr;
}



void pthread_mgr_t::add_to_pcond_schedule(pcond_ctx_t *ctx)
{
    if (!list_empty(&ctx->wait_item))
    {
        SCOPE_LOCK(&ctx->mgr->mutex)
        {
            list_del_init(&ctx->wait_item);
        }
    }

    llist_add(&ctx->link_to_schedule, &pcond_schedule_queue);
    pcond_event_notify.notify(1);
}


pthread_mgr_t::pthread_mgr_t(coroutine_env_t *env)
{
    this->co_env = env;
    int ret = pcond_event_notify.init(pcond_event_cb, this);
    if (ret) {
        //致命错误
        fprintf(stderr, "init pthread mgr pcond_event_notify failed; ret:%d", ret); 
        abort();
    }

    init_llist_head(&pcond_schedule_queue);
}

int pthread_mgr_t::pcond_event_cb(void *arg, uint64_t num)
{
    pthread_mgr_t * self = (pthread_mgr_t *)(arg);
    if (num > 0) {
        int cnt =0;

        llist_node * queue = llist_del_all(&self->pcond_schedule_queue);

        if ( queue ) {
            // 之前是先进后出, 倒序后, 就是先进先出了
            queue = llist_reverse_order(queue);

            pcond_ctx_t * ctx = NULL, *next = NULL;
            llist_for_each_entry_safe(ctx, next, &queue, link_to_schedule)
            {
                ctx->link_to_schedule.next=NULL;
                cancel_timeout(&ctx->tm);
                conet::resume(ctx->co);
                ++cnt;
            }
        }
    }
    return 0;
}

pthread_mgr_t::~pthread_mgr_t()
{
    pcond_event_notify.stop();
}

}



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

        conet::AddrMap *map = NULL;
        {
            conet::DisablePthreadHook disable;
            map = new conet::AddrMap();
        }

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

    pcond_mgr_t * mgr = NULL;
    {
        conet::DisablePthreadHook disable;
        mgr = new pcond_mgr_t(key);
    }

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

    pthread_mgr_t *pmgr = get_pthread_mgr();

    pcond_ctx_t ctx;

    ctx.co = conet::current_coroutine();

    pcond_mgr_t *mgr = find_cond_map(cond);
    ctx.mgr = mgr;
    SCOPE_LOCK(&mgr->mutex)
    { 
        list_add_tail(&ctx.wait_item, &mgr->wait_list);
    }

    ctx.pmgr = pmgr;

    if (mutex) pthread_mutex_unlock(mutex);
    conet::yield();
    if (mutex) pthread_mutex_lock(mutex);

    return 0;
}


static
void proc_pthread_cond_timeout(void *arg)
{
    pcond_ctx_t *ctx = (pcond_ctx_t *) (arg);
    ctx->ret_timeout = 1;

    ctx->pmgr->add_to_pcond_schedule(ctx);
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

    pcond_ctx_t ctx;
    if(abstime)
    {
        if (abstime->tv_sec <= 0) return EINVAL;
        if (abstime->tv_nsec >= 1000000000) return EINVAL;

        uint64_t timeout = (abstime->tv_sec*1000 + abstime->tv_nsec/1000000);
        uint64_t now = conet::get_sys_ms();  // 必须是系统的时间
        if (timeout < now) timeout = 0;
        timeout -= now;

        init_timeout_handle(&ctx.tm, proc_pthread_cond_timeout, &ctx);
        set_timeout(&ctx.tm, timeout);
    }

    pthread_mgr_t *pmgr = get_pthread_mgr();

    ctx.co = conet::current_coroutine();
    INIT_LIST_HEAD(&ctx.wait_item);

    pcond_mgr_t *mgr = find_cond_map(cond);

    ctx.mgr = mgr;
    SCOPE_LOCK(&mgr->mutex)
    {
        list_add_tail(&ctx.wait_item, &mgr->wait_list);
    }

    ctx.pmgr = pmgr;

    if (mutex) pthread_mutex_unlock(mutex);
    conet::yield();
    if (mutex) pthread_mutex_lock(mutex);
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

        pcond_ctx_t *ctx=NULL, *next=NULL;
        list_for_each_entry_safe(ctx, next, &notify_queue, wait_item)
        {
            ++cnt;
            ctx->pmgr->add_to_pcond_schedule(ctx);
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

HOOK_FUNC_DEF(int, pthread_join, (pthread_t tid, void **retval))
{
    HOOK_FUNC(pthread_join);
    if (!conet::is_enable_pthread_hook())
    {
        return _(pthread_join)(tid, retval);
    }

    while(1)
    {
        int ret = pthread_tryjoin_np(tid, retval);
        if (ret == 0)  {
            return ret;
        } else {
            conet::delay_back();
        }
    }
    return 0;
}

HOOK_FUNC_DEF(int, pthread_timedjoin_np, (pthread_t tid, void **retval, 
            const struct timespec * abstime))
{
    HOOK_FUNC(pthread_timedjoin_np);
    if (!conet::is_enable_pthread_hook())
    {
        return _(pthread_timedjoin_np)(tid, retval, abstime);
    }

    if (abstime->tv_sec <= 0) return EINVAL;
    if (abstime->tv_nsec >= 1000000000) return EINVAL;

    uint64_t timeout = (abstime->tv_sec*1000 + abstime->tv_nsec/1000000);
    while(1)
    {
        int ret = pthread_tryjoin_np(tid, retval);
        if (ret == 0)  {
            return ret;
        } else {
            conet::delay_back();
            if (timeout < conet::get_sys_ms()) {
                return ETIMEDOUT;
            }
        }
    }
    return 0;
}

// 锁相关
//

/*
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

void pthread_mgr_t::add_lock_schedule(lock_ctx_t *ctx) 
{
    list_add_tail(&ctx->wait_item, &lock_schedule_queue);
    if (list_empty(&schedule_task.link_to)) {
        conet::registry_delay_task(&schedule_task);
    }
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

    mgr->add_lock_schedule(&ctx);
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

    mgr->add_lock_schedule(&ctx);

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

    mgr->add_lock_schedule(&ctx);
    conet::yield();
    return 0;
}
*/

/*  自旋锁， 没必要hook, 因为这个本来就很快

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

    mgr->add_lock_schedule(&ctx);

    conet::yield();
    return 0;
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
*/
