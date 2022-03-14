/*
 * =====================================================================================
 *
 *       Filename:  common.cpp
 *
 *    Description:
 *
 *        Version:  1.0
 *        Created:  03/24/2015 02:40:12 PM
 *       Revision:  none
 *       Compiler:  gcc
 *
 *         Author:  piboye
 *   Organization:
 *
 * =====================================================================================
 */
#include <stdlib.h>
#include <pthread.h>
#include <unistd.h>
#include <sys/types.h>
#include <sys/syscall.h>

#include "./common.h"
#include "./time_mgr.h"
#include "coroutine_env.h"
#include "fd_ctx.h"
#include "log.h"
namespace conet
{
    static int g_cenv_inited = 0; 
    int init_conet_global_env()
    {
        int ret = 0;
	if (g_cenv_inited) {
	   return 0;
	}
	g_cenv_inited = 1;
        ret = time_mgr_t::instance().start();
        if (ret) {
            PLOG_FATAL("init time mgr failed! [ret=", ret, "]");
            return -1;
        }
        g_coroutine_envs = new coroutine_env_t*[102400];

	conet::init_fd_ctx_env();
        return 0;
    }

    int free_conet_global_env()
    {
        int ret = 0;
        ret = time_mgr_t::instance().stop();
        if (ret) {
            PLOG_FATAL("free time mgr failed! [ret=", ret, "]");
            return -1;
        }
        delete g_coroutine_envs;
        return 0;
    }

    uint64_t get_local_tid()
    {
        static __thread uint64_t tid = 0;
        if (tid > 0) return tid;
        //pid_t pid = getpid();
        tid = syscall(__NR_gettid);
        //tid = tid2-pid;
        return tid;
    }

    int init_conet_env()
    {
        if (g_coroutine_env) {
            PLOG_INFO("duplicate init coroutine env");
            return 0;
        }
        uint64_t tid = get_local_tid();
        coroutine_env_t *env = new coroutine_env_t();
        g_coroutine_env = env;
        g_coroutine_envs[tid] = env;
        env->tw->start();
        return 0;
    }

    int free_conet_env()
    {
        if (NULL == g_coroutine_env) {
            //PLOG_FATAL("coroutine env don't init , free is bug!");
            return -1;
        }

        uint64_t tid = get_local_tid();
        delete g_coroutine_env;
        g_coroutine_env = NULL;
        g_coroutine_envs[tid] = NULL;
        return 0;
    }

    static int init_cg_env=conet::init_conet_global_env();
    static int init_c_env=conet::init_conet_env();
}


