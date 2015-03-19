/*
 * =====================================================================================
 *
 *       Filename:  struct2.h
 *
 *    Description:  
 *
 *        Version:  1.0
 *        Created:  03/19/2015 09:34:54 PM
 *       Revision:  none
 *       Compiler:  gcc
 *
 *         Author:  piboye
 *   Organization:  
 *
 * =====================================================================================
 */
#ifndef __STRUCT2_H__
#define __STRUCT2_H__
#include "macro_help.h"
#include "jsoncpp/json.h"
#include "jsoncpp/value.h"

namespace conet
{

// Struct To Json::Value

#define BASE_TYPE_STRUCT_TO_JSON(type) \
template <typename t> \
inline \
void struct_to_json(Json::Value *out, type const &v) \
{ \
    Json::Value vn(v); \
    out->swap(vn); \
    return out; \
} \

BASE_TYPE_MAP_TO_JSON(int)
BASE_TYPE_MAP_TO_JSON(unsigned int)
BASE_TYPE_MAP_TO_JSON(long)
BASE_TYPE_MAP_TO_JSON(unsigned long)
BASE_TYPE_MAP_TO_JSON(long long)
BASE_TYPE_MAP_TO_JSON(unsigned long long)
BASE_TYPE_MAP_TO_JSON(float)
BASE_TYPE_MAP_TO_JSON(double)
BASE_TYPE_MAP_TO_JSON(long double)
BASE_TYPE_MAP_TO_JSON(std::string)
BASE_TYPE_MAP_TO_JSON(char *)
BASE_TYPE_MAP_TO_JSON(char const *)

template <typename t>
inline
void struct_to_json(Json::Value *out, std::vector<t> const & vec) 
{
    Json::Value vn(Json::arrayValue);
	for (size_t i=0, len=vec.size(); i<len; ++i)
    {
       Json::Value v;
       vn.append(v);
       struct_to_json(&vn[vn.size()-1], vec[i]);
	}
    out->swap(vn);
}


#define struct_to_json_attr2(out, name, value) \
    do { \
        (*out)[name].swap(*struct_to_json(value)); \
    } while(0)\

template <typename t>
inline
void struct_to_json(Json::Value *out, std::map<std::string, t> const & map)
{
    Json::Value vn (objectValue);
    for (typeof(map.begin()) it = map.begin(), iend = map.end();
        it!=iend; ++it)
    {
       struct_to_json_attr2(&vn[it->first], it->first, it->second);
    }
    out->swap(vn);
}

template <typename t>
inline
void struct_to_json(Json::Value *out, std::map<char *, t> const & map)
{
    Json::Value vn (objectValue);
    for (typeof(map.begin()) it = map.begin(), iend = map.end();
        it!=iend; ++it)
    {
       struct_to_json_attr2(&vn[it->first], it->first, it->second);
    }
    out->swap(vn);
}

template <typename t>
inline
void struct_to_json(Json::Value *out, std::map<char const *, t> const & map)
{
    Json::Value vn (objectValue);
    for (typeof(map.begin()) it = map.begin(), iend = map.end();
        it!=iend; ++it)
    {
       struct_to_json_attr2(&vn[it->first], it->first, it->second);
    }
    out->swap(vn);
}

template <typename t>
inline
void struct_to_json(Json::Value *out, t const &v) {
    return v.struct_to_json(out);
}

#define CONET_STURCT_TO_JSON_MEM_CALL(r, out, i, name) \
    struct_to_json(&out[BOOST_PP_STRINGIZE(name)], v.name); \

#define DEF_STURCT_TO_JSON(type, param) DEF_STRUCT_TO_JSON_IMPL(type, BOOST_PP_VARIADIC_TO_SEQ param)

#define DEF_TO_JSON_IMPL(type, seq_param)\
inline  \
void struct_to_json(Json::Value *out, type const & v) \
{ \
    Json::Value obj(Json::objectValue); \
    BOOST_PP_SEQ_FOR_EACH_I(CONET_TO_JSON_MEM_CALL, obj, seq_param) \
    out->swap(obj); \
} \

#define DEF_TO_JSON_MEM(param) DEF_TO_JSON_MEM_IMPL(BOOST_PP_VARIADIC_TO_SEQ param)

#define DEF_TO_JSON_MEM_IMPL(seq_param) \
inline \
void struct_to_json(Json::Value *out) const \
{ \
    typeof(*this) const & v = *this; \
    Json::Value obj(Json::objectValue); \
    BOOST_PP_SEQ_FOR_EACH_I(CONET_TO_JSON_MEM_CALL, obj, seq_param) \
    out->swap(obj); \
} \

}

#endif /* end of include guard */
