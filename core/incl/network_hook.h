/*
 * =====================================================================================
 *
 *       Filename:  network_hook.h
 *
 *    Description:
 *
 *        Version:  1.0
 *        Created:  2014年05月21日 17时09分00秒
 *       Revision:  none
 *       Compiler:  gcc
 *
 *         Author:  piboye
 *   Organization:
 *
 * =====================================================================================
 */
#ifndef __NETWORK_HOOK_H__
#define __NETWORK_HOOK_H__

namespace conet
{

void disable_sys_hook();
void enable_sys_hook();
int is_enable_sys_hook();

}

#endif /* end of include guard */
