/*
 * =====================================================================================
 *
 *       Filename:  to_lua_mgr.h
 *
 *    Description:  
 *
 *        Version:  1.0
 *        Created:  2014年12月10日 11时23分03秒
 *       Revision:  none
 *       Compiler:  gcc
 *
 *         Author:  piboye
 *   Organization:  
 *
 * =====================================================================================
 */

#ifndef __CONET_TO_LUA_MGR_H__
#define __CONET_TO_LUA_MGR_H__


namespace conet
{

int registry_to_lua(int (*f)(luaState *l));

int construct_to_lua(luaState *l);

}

#endif /* end of include guard */
