/*
 * =====================================================================================
 *
 *       Filename:  func_wrap_mgr.h
 *
 *    Description:
 *
 *        Version:  1.0
 *        Created:  01/14/2015 09:50:55 AM
 *       Revision:  none
 *       Compiler:  gcc
 *
 *         Author:  piboye
 *   Organization:
 *
 * =====================================================================================
 */

#ifndef __CONET_FUNC_WRAP_MGR_H__
#define __CONET_FUNC_WRAP_MGR_H__

namespace conet
{

FuncWrapData * get_func_wrap_data();
void free_func_wrap_data(FuncWrapData *d);
}

#endif /* end of include guard */

