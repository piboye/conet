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

#include <vector>
#include <string>

struct lua_State;

namespace conet
{

class ToLuaMgr
{
public:
    class Item
    {
        public:
        int (*f)(lua_State *l);
        int line;
        std::string file;
        std::string name;
    };

    std::vector<Item> m_to_luas;

    int Add(int (*f)(lua_State *l), 
            char const * name, char const * file,  int line);

    int ToLua(lua_State *l);

    static
    int GlobalAdd(int (*f)(lua_State *l),
            char const * name, char const * file,  int line);

    static
    int GlobalToLua(lua_State *l);
};


#define REG_TO_LUA_HELP(name, lua_state) \
    static int conet_reg_to_lua_help_func_##name##__LINE__(lua_State *lua_state); \
    static int conet_reg_to_lua_help_var_##name##__LINE__ = ToLuaMgr::GlobalAdd(conet_reg_to_lua_help_func_##name##__LINE__,#name, __FILE__, __LINE__); \
    static int conet_reg_to_lua_help_func_##name##__LINE__(lua_State *lua_state) \
    
 

}

#endif /* end of include guard */
