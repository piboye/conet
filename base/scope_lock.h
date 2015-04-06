/*
 * =====================================================================================
 *
 *       Filename:  scope_lock.h
 *
 *    Description:  
 *
 *        Version:  1.0
 *        Created:  04/06/2015 05:57:54 PM
 *       Revision:  none
 *       Compiler:  gcc
 *
 *         Author:  piboye
 *   Organization:  
 *
 * =====================================================================================
 */
#ifndef __CONET_SCOPE_LOCK_H__
#define __CONET_SCOPE_LOCK_H__

#include <pthread.h>

namespace conet 
{
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
        pthread_mutex_lock(m);

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
        pthread_rwlock_rdlock(l);

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
        pthread_rwlock_wrlock(l);
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

#endif /* end of include guard */
