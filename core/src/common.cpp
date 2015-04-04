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
#include "./common.h"
#include "./time_mgr.h"
#include "coroutine_env.h"
#include "log.h"
namespace conet
{
    int init_conet_global_env()
    {
        int ret = 0;
        ret = time_mgr_t::instance().start();
        return 0;
    }

    int free_conet_global_env()
    {
        int ret = 0;
        ret = time_mgr_t::instance().stop();
        return 0;
    }

    int init_conet_env()
    {
        if (g_coroutine_env) {
            LOG(FATAL)<<"duplicate init coroutine env";
            return -1;
        }
        g_coroutine_env = new coroutine_env_t();
        g_coroutine_env->tw->start();
        return 0;
    }

    int free_conet_env()
    {
        if (NULL == g_coroutine_env) {
            LOG(FATAL)<<"coroutine env don't init , free is bug!";
            return -1;
        }

        delete g_coroutine_env;
        g_coroutine_env = NULL;
        return 0;
    }
}

