/*
 * =====================================================================================
 *
 *       Filename:  common.h
 *
 *    Description:  
 *
 *        Version:  1.0
 *        Created:  03/24/2015 02:39:58 PM
 *       Revision:  none
 *       Compiler:  gcc
 *
 *         Author:  piboye
 *   Organization:  
 *
 * =====================================================================================
 */
#ifndef __CONET_COMMON_H__
#define __CONET_COMMON_H__
#include "base/list.h"

namespace conet
{
    int init_conet_global_env();
    int free_conet_global_env();

    int init_conet_env();
    int free_conet_env();
}
#endif /* end of include guard */
