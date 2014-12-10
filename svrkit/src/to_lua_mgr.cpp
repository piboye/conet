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


class ToLuaMgr
{
public:
    class Item
    {
        public:
        int (*f)(luaState *l);
        int line;
        std::string file;
        std::string name;
    };

    std::vector<Item> m_to_luas;

    int Add(int (*f)(luaState *l), 
            char const * name, char const * file,  int line)
    {
        Item item;
        item.f = f;
        item.file = file;
        item.name = name;
        item.line = line;
    }
}

int registry_to_lua(int (*f)(luaState *l))

int construct_to_lua(luaState *l);
}

