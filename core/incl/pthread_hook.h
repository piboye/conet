/*
 * =====================================================================================
 *
 *       Filename:  pthread_hook.h
 *
 *    Description:
 *
 *        Version:  1.0
 *        Created:  2014年05月21日 17时07分33秒
 *       Revision:  none
 *       Compiler:  gcc
 *
 *         Author:  YOUR NAME (),
 *   Organization:
 *
 * =====================================================================================
 */
#ifndef __PTHREAD_HOOK_H__
#define __PTHREAD_HOOK_H__

namespace conet
{

int is_enable_pthread_hook();
void enable_pthread_hook();
void disable_pthread_hook();

}

#endif /* end of include guard */
