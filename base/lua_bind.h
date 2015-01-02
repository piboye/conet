/*
 * =====================================================================================
 *
 *       Filename:  lua_bind.h
 *
 *    Description:  
 *
 *        Version:  1.0
 *        Created:  2014年12月10日 06时31分10秒
 *       Revision:  none
 *       Compiler:  gcc
 *
 *         Author:  piboye
 *   Organization:  
 *
 * =====================================================================================
 */

#ifndef __LUA_BIND_H__
#define __LUA_BIND_H__

#include "thirdparty/glog/logging.h"

extern "C"
{
    #include <lua.h>
}

#include <string>

namespace conet 
{
template<typename T>
T lua_get_param(lua_State *state, int index)
{
    LOG(ERROR)<<"parement type unsupport type";
    return T();
}

template <>
int lua_get_param<int>(lua_State *state, int index)
{
    if (!lua_isnumber(state, index)) {
        LOG(ERROR)<<"lua argument should be a number";
        return 0;
    }

    return (int)lua_tonumber(state, index);
}


template <>
bool lua_get_param<bool>(lua_State *state, int index)
{
    if (!lua_isboolean(state, index)){
        LOG(ERROR)<<"lua argument should be a boolean";
        return 0;
    }

    return (bool)lua_toboolean(state, index);
}


template <>
double lua_get_param<double>(lua_State *state, int index)
{
    if (!lua_isnumber(state, index)) {
        LOG(ERROR)<<"lua argument should be a number";
        return 0;
    }

    return lua_tonumber(state, index);
}


template <>
char const * lua_get_param<char const*>(lua_State *state, int index)
{
    if (!lua_isstring(state, index)){
        LOG(ERROR)<<"lua argument should be a string";
        return "";
    }

    return lua_tostring(state, index);
}


template <>
std::string lua_get_param<std::string >(lua_State *state, int index)
{
    if (!lua_isstring(state, index)){
        LOG(ERROR)<<"lua argument should be a string";
        return std::string();
    }

    return std::string(lua_tostring(state, index));
}

 

template<typename T>
int lua_set_result(lua_State *state, T value)
{
    LOG(ERROR)<<"parement type unsupport type";
    return 0;
}

template <>
int lua_set_result<int>(lua_State *state, int value)
{
    lua_pushnumber(state, double(value));
    return 1;
}

template <>
int lua_set_result<bool>(lua_State *state, bool value)
{
    lua_pushboolean(state, value);
    return 1;
}

template <>
int lua_set_result<double>(lua_State *state, double value)
{
    lua_pushnumber(state, value);
    return 1;
}


template <>
int lua_set_result<char const *>(lua_State *state, char const * value)
{
    if(value) {
        lua_pushstring(state, value);
    } else {
        lua_pushstring(state, "");
    }
    return 1;
}


template <>
int lua_set_result<std::string>(lua_State *state, std::string value)
{
    lua_pushstring(state, value.c_str());
    return 1;
}

template<typename T>
int lua_set_result(lua_State *state, T* value)
{
    return 0;
}


#include "repeat_macro.h"

template<typename T>
class function_type_info
{
    public:
        typedef T result_type; 
};

#define TEMPLATE_PARAM_TYPE(s, v, arg) COMMA_IF(v) typename CAT(arg, v) 
#define TEMPLATE_PARAM_TYPE_LIST(num) EXPR(0)(REPEAT(0, num, TEMPLATE_PARAM_TYPE, arg))

#define DEF_FUN_PARAM_TYPE(s, v, arg) typedef CAT(arg, v) CAT(CAT(arg, v), _type) ;
#define ALL_DEF_FUN_PARAM_TYPE(num) EXPR(0)(REPEAT(0, num, DEF_FUN_PARAM_TYPE, arg))

#define GET_FUN_PARAM_TYPE(s, v, arg) COMMA_IF(v) CAT(arg, INC(v))
#define GET_FUN_PARAM_TYPE_LIST(num) EXPR(0) (REPEAT(0, num, GET_FUN_PARAM_TYPE, arg))

#define DEF_FUNCTION_TYPE_INFO(_, num, arg) \
template< TEMPLATE_PARAM_TYPE_LIST(INC(num)) > \
class function_type_info< arg0 (*)( GET_FUN_PARAM_TYPE_LIST(num) ) > \
{\
    public:\
    ALL_DEF_FUN_PARAM_TYPE(INC(num))\
    typedef arg0 result_type;\
};


DEF_FUNCTION_TYPE_INFO(_, 0, arg)
DEF_FUNCTION_TYPE_INFO(_, 1, arg)
DEF_FUNCTION_TYPE_INFO(_, 2, arg)
DEF_FUNCTION_TYPE_INFO(_, 3, arg)
DEF_FUNCTION_TYPE_INFO(_, 4, arg)
DEF_FUNCTION_TYPE_INFO(_, 5, arg)
DEF_FUNCTION_TYPE_INFO(_, 6, arg)
DEF_FUNCTION_TYPE_INFO(_, 7, arg)
DEF_FUNCTION_TYPE_INFO(_, 8, arg)
DEF_FUNCTION_TYPE_INFO(_, 9, arg)
DEF_FUNCTION_TYPE_INFO(_, 10, arg)
DEF_FUNCTION_TYPE_INFO(_, 11, arg)
DEF_FUNCTION_TYPE_INFO(_, 12, arg)
DEF_FUNCTION_TYPE_INFO(_, 13, arg)
DEF_FUNCTION_TYPE_INFO(_, 14, arg)


#define LUA_PARAM_TYPE(functor,n)  CAT(CAT(functor::arg, n),_type)
#define GET_PARAM_IN(s, v, functor)  LUA_PARAM_TYPE(functor, v) CAT(arg, v) = conet::lua_get_param< LUA_PARAM_TYPE(functor, v) >(state, v);
#define GET_PARAM_FUN(n, functor) EXPR(0)(REPEAT(1, INC(n), GET_PARAM_IN, functor))


#define LUA_CALL_ARG_IN(s, v, arg) COMMA_IF(v) CAT(arg, INC(v))
#define LUA_CALL_FUN_ARG(n, arg) EXPR(0)(REPEAT(0, n, LUA_CALL_ARG_IN, arg))

#define LUA_BIND_NAME(fun, num) CAT(CAT(__lua_bind_, fun), num)
#define DEF_LUA_BIND(fun, num)\
    int LUA_BIND_NAME(fun, num) (lua_State *state){ \
        typedef conet::function_type_info<typeof(&fun)> functor_info_type; \
        GET_PARAM_FUN(num, functor_info_type) \
        typeof(fun(LUA_CALL_FUN_ARG(num, arg))) result = fun(LUA_CALL_FUN_ARG(num, arg));\
        int ret = conet::lua_set_result(state, result); \
        return ret;\
    }


#define DEF_LUA_BIND_VOID(fun, num)\
    int LUA_BIND_NAME(fun, num) (lua_State *state){ \
        typedef conet::function_type_info<typeof(&fun)> functor_info_type; \
        GET_PARAM_FUN(num, functor_info_type) \
        fun(LUA_CALL_FUN_ARG(num, arg));\
        return 0;\
    }

}

#endif //__LUA_BIND_H__
