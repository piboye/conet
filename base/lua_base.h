/*
 * =====================================================================================
 *
 *       Filename:  lua_base.h
 *
 *    Description:  
 *
 *        Version:  1.0
 *        Created:  2014年12月10日 06时37分03秒
 *       Revision:  none
 *       Compiler:  gcc
 *
 *         Author:  piboye
 *   Organization:  
 *
 * =====================================================================================
 */

#ifndef __LUA_BASE_H__
#define __LUA_BASE_H__

extern "C"
{
    #include <lua.h>
    #include <lauxlib.h>
    #include <lualib.h>
}

#include "lua_bind.h"

#include <sstream>
#include <string>
#include <assert.h>

namespace conet
{

class LuaBase
{
public:
    bool open(char const *filename) {
        if(NULL == filename) return false;
        file_name_ = filename;

        state_ = lua_open();
        luaopen_base(state_);
        //luaopen_io(state_);
        //luaopen_string(state_);
        //luaopen_math(state_);
        if (luaL_loadfile(state_, file_name_.c_str())) {
            LOG(ERROR)<<"cannot load configuration file:"<<file_name_;
            close();
            return false;
        }
        return true;
    }

    typedef int bind_type(lua_State *);

    bool registry(char const *name, bind_type fun) {
        if(NULL==name || NULL == state_) return false;
        lua_pushcfunction(state_, fun);
        lua_setglobal(state_,  name);
        return true;
    }

    bool load()
    {
      if(NULL == state_) return false;
      if(lua_pcall(state_, 0, 0, 0)){
         LOG(ERROR)<<"cannot run configuration file: %s", file_name_;
         close();
         return false;
      }
      return true;
    }


    void close() {
        if(state_) lua_close(state_);
        state_ = NULL;
    }

    lua_State *state_;
    std::string file_name_;
};

class LuaConf: public LuaBase
{
    public:
        template <typename T>
        T get(char const* name){
            if(NULL==name || NULL == state_) return T();
            lua_getglobal(state_, name);
            if (!lua_isstring(state_, -1)){
                LOG(ERROR)<<"should be a string";
                return T();
            }

            std::string value = lua_tostring(state_, -1);
            lua_pop(state_, -1);
            std::istringstream in(value, std::istringstream::in);
            T ret;
            in>>ret;
            return ret;
        }

};

template <>
int LuaConf::get<int>(char const* name){
    if(NULL==name || NULL == state_) return 0;
    lua_getglobal(state_, name);
    if (!lua_isnumber(state_, -1)) {
        LOG(ERROR)<<"should be a number";
        return 0;
    }

    int result = (int)lua_tonumber(state_, -1);
    lua_pop(state_, -1);
    return result;
}

template <>
double LuaConf::get<double>(char const* name) {
    if(NULL==name || NULL == state_) return 0;
    lua_getglobal(state_, name);
    if (!lua_isnumber(state_, -1)) {
        LOG(ERROR)<<"should be a number";
        return 0;
    }

    double result = lua_tonumber(state_, -1);
    lua_pop(state_, -1);
    return result;
}

template <>
std::string LuaConf::get<std::string>(char const* name) {
    if(NULL==name || NULL == state_) return std::string();
    lua_getglobal(state_, name);
    if (!lua_isstring(state_, -1)) {
        LOG(ERROR)<<"should be a string";
        return std::string();
    }

    std::string result =  lua_tostring(state_, -1);
    lua_pop(state_, -1);
    return result;
}

};

#endif 
