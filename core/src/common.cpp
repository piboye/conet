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

}

