/*
 * =====================================================================================
 *
 *       Filename:  to_lua_mgr.cpp
 *
 *    Description:  
 *
 *        Version:  1.0
 *        Created:  2014年12月10日 11时26分48秒
 *       Revision:  none
 *       Compiler:  gcc
 *
 *         Author:  piboye
 *   Organization:  
 *
 * =====================================================================================
 */

#include <stdlib.h>

#include "to_lua_mgr.h"

namespace conet
{

    static ToLuaMgr * get_global_to_lua_mgr()
    {

        static ToLuaMgr * g_to_lua_mgr = new ToLuaMgr();
        return g_to_lua_mgr;
    }

    int ToLuaMgr::Add(int (*f)(lua_State *l), 
            char const * name, char const * file,  int line)
    {
        Item item;
        item.f = f;
        item.file = file;
        item.name = name;
        item.line = line;
        m_to_luas.push_back(item);
        return 0;
    }

    int ToLuaMgr::ToLua(lua_State *l)
    {
        for (size_t i=0; i<m_to_luas.size(); ++i)
        {
           m_to_luas[i].f(l);
        }
        return 0;
    }

    int ToLuaMgr::GlobalAdd(int (*f)(lua_State *l),
            char const * name, char const * file,  int line)
    {
        return get_global_to_lua_mgr()->Add(f, name, file, line);
    }

    int ToLuaMgr::GlobalToLua(lua_State *l)
    {
        return get_global_to_lua_mgr()->ToLua(l);
    }


}

