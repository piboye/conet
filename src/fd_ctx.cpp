/*
 * =====================================================================================
 *
 *       Filename:  fd_ctx.cpp
 *
 *    Description:  
 *
 *        Version:  1.0
 *        Created:  2014年05月06日 15时55分54秒
 *       Revision:  none
 *       Compiler:  gcc
 *
 *         Author:  piboyeliu
 *   Organization:  
 *
 * =====================================================================================
 */
#include <stdlib.h>
#include "fd_ctx.h"
#include <unistd.h>
#include <fcntl.h>
#include <dlfcn.h>
#include <assert.h>
#include "log.h"

typedef int (*fcntl_pfn_t)(int fildes, int cmd, ...);
extern fcntl_pfn_t g_sys_fcntl_func;
#define HOOK_SYS_FUNC(name) if( !g_sys_##name##_func ) { g_sys_##name##_func = (name##_pfn_t)dlsym(RTLD_NEXT,#name); }

namespace conet
{

static fd_ctx_t * g_fd_ctxs_cache[2048000]={0};

static fd_ctx_t **g_fd_ctxs = g_fd_ctxs_cache;

static int g_fd_ctx_len = 2048000; 

fd_ctx_t *get_fd_ctx(int fd)
{
    if (fd <0) return NULL;
    if (fd >= g_fd_ctx_len ) {
        assert("!too many fd");
        exit(1);
    }
    return  g_fd_ctxs[fd];
}

fd_ctx_t *alloc_fd_ctx(int fd) 
{
    if (fd <0) {
        assert(!"fd <0");
        exit(1);
        return NULL;
    }
    
    if (fd >= g_fd_ctx_len ) {
        assert("!too many fd");
        exit(1);
    }

    fd_ctx_t * d = g_fd_ctxs[fd];
    if (NULL == d) 
    {
        HOOK_SYS_FUNC(fcntl);
        d = ( fd_ctx_t *) malloc(sizeof(fd_ctx_t));
        d->fd = fd;
        d->use_cnt = 1;
        INIT_LIST_HEAD(&d->poll_wait_queue);
        d->rcv_timeout = 1000;
        d->snd_timeout = 1000;
        int flags = 0;
        flags = g_sys_fcntl_func(fd, F_GETFL,0 );
        d->user_flag = flags;

        // 设置 none block, 方便hook 系统调用
        // user_flag 只保存用户设置的标志。 
	    g_sys_fcntl_func(fd, F_SETFL, flags | O_NONBLOCK); 

        //for multi thread access;
        if (!__sync_bool_compare_and_swap((g_fd_ctxs+fd), NULL, d))
        {
            if (d)  free(d);
        }
    }
    return g_fd_ctxs[fd];
}

int free_fd_ctx(int fd)
{
    if (fd <0) return -1;
    fd_ctx_t * d = g_fd_ctxs[fd];
    if (d == NULL) return 0;

    //for multi thread access;
    if (!__sync_bool_compare_and_swap((g_fd_ctxs+fd), d, NULL))
    {
        return 0; 
    }
    decr_ref_fd_ctx(d);
    return 0;
}

void incr_ref_fd_ctx(fd_ctx_t *obj)
{
    ++obj->use_cnt;
}

void decr_ref_fd_ctx(fd_ctx_t *obj)
{
    --obj->use_cnt;
    if (obj->use_cnt <= 0) {
        free(obj);
    }
}

}
