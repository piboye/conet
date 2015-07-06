/*
 * =====================================================================================
 *
 *       Filename:  json_to.h
 *
 *    Description:  
 *
 *        Version:  1.0
 *        Created:  07/06/2015 01:00:03 AM
 *       Revision:  none
 *       Compiler:  gcc
 *
 *         Author:  piboye
 *   Organization:  
 *
 * =====================================================================================
 */


#ifndef __JSON2STRUCT_H__
#define __JSON2STRUCT_H__
#include "macro_help.h"
#include "jsoncpp/json.h"
#include "jsoncpp/value.h"
#include <string>
#include <vector>
#include "string2number.h"

#define DEF_BASETYPE_JSON_TO_NUMBER(type) \
inline \
int json_to(Json::Value const & json_value, type * val) \
{ \
    return conet::string2number(json_value.asString(), val); \
}

DEF_BASETYPE_JSON_TO_NUMBER(int8_t);
DEF_BASETYPE_JSON_TO_NUMBER(uint8_t);
DEF_BASETYPE_JSON_TO_NUMBER(int16_t);
DEF_BASETYPE_JSON_TO_NUMBER(uint16_t);
DEF_BASETYPE_JSON_TO_NUMBER(int32_t);
DEF_BASETYPE_JSON_TO_NUMBER(uint32_t);
DEF_BASETYPE_JSON_TO_NUMBER(int64_t);
DEF_BASETYPE_JSON_TO_NUMBER(uint64_t);
DEF_BASETYPE_JSON_TO_NUMBER(float);
DEF_BASETYPE_JSON_TO_NUMBER(double);

inline int json_to(Json::Value const & json_value, std::string * val)
{
    *val = json_value.asString();
    return 0;
}

template <typename T>
inline int json_to(Json::Value const & json_value, std::vector<T> * val)
{
    if (!json_value.isArray()) {
        return -1;
    }
    for (int i=0, size = (int)json_value.size(); i< size; ++i)
    {
        T v1;
        json_to(json_value[i], &v1);
        val->push_back(v1);
    }
    return 0;
}

template <typename T>
inline 
int json_to(Json::Value const & json_val, T * val)
{
    return val->json_to_self(json_val);
}

template <typename T>
inline int json_to(std::string const & txt, T * val)
{
    Json::Value json_value;
    Json::Reader reader;
    if (!reader.parse(txt, json_value)) {
        return -2;
    }
    return json_to(json_value, val);
}

#define CONET_JSON_TO_ATTR_CALL(r, out, name) \
    json_to(json_value[BOOST_PP_STRINGIZE(name)], &data->name);

#define DEF_JSON_TO(struct_, members) \
    DEF_JSON_TO_IMPL(struct_, BOOST_PP_VARIADIC_TO_SEQ members)


#define DEF_JSON_TO_IMPL(struct_, seq_members) \
inline  \
int json_to(Json::Value const & json_value, struct_ * data) \
{ \
  if (!json_value.isObject()) { \
      return -1; \
  } \
  BOOST_PP_SEQ_FOR_EACH(CONET_JSON_TO_ATTR_CALL, json_value, seq_members); \
  return 0; \
} \


#define DEF_JSON_TO_MEMBER(members) \
    DEF_JSON_TO_MEMBER_IMPL(BOOST_PP_VARIADIC_TO_SEQ members)


#define DEF_JSON_TO_MEMBER_IMPL(seq_members) \
inline  \
int json_to_self(Json::Value const & json_value) \
{ \
  if (!json_value.isObject()) { \
      return -1; \
  } \
  typeof(this) data = this; \
  BOOST_PP_SEQ_FOR_EACH(CONET_JSON_TO_ATTR_CALL, json_value, seq_members); \
  return 0; \
} \

#endif /* end of include guard */
