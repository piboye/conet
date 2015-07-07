#ifndef __CONET_TO_JSON_H__
#define __CONET_TO_JSON_H__

#include <string>
#include <vector>
#include <map>

#include <stdint.h>
#include <stdlib.h>
#include <string.h>
#include <stdio.h>

#include "macro_help.h"


#define CONET_JSON_ENCODE_CHAR(ch, alter) \
case ch : \
    out.append(alter); \
    break

inline
void json_encode(std::string &sString)
{
    int length = sString.length();
    std::string out;
    char* it = (char*) sString.c_str();
    for (int i = 0; i < length; i++) 
    {
        char ch = *it;
        switch (ch)
        {
            CONET_JSON_ENCODE_CHAR('<', "&lt;");
            CONET_JSON_ENCODE_CHAR('>', "&gt;");
            CONET_JSON_ENCODE_CHAR('\"', "&quot;");
            CONET_JSON_ENCODE_CHAR('\'', "&#39;");
            CONET_JSON_ENCODE_CHAR('\\', "&#92;");
            CONET_JSON_ENCODE_CHAR('\n', "\\n");
            CONET_JSON_ENCODE_CHAR('\r', "\\r");
            CONET_JSON_ENCODE_CHAR('\t', "\\t");

            default:
                out.push_back((char)ch);
        }
        ++it;
    }
    sString.swap(out);
    return;
}

#define DEF_BASETYPE_TO_JSON(fmt, type) \
inline \
void to_json_value_help2(std::string &out, type value) { \
    char buffer[40]; \
    snprintf(buffer, sizeof(buffer), fmt, value); \
    out.append(buffer); \
} \



DEF_BASETYPE_TO_JSON("%d", int)
DEF_BASETYPE_TO_JSON("%u", unsigned int)
DEF_BASETYPE_TO_JSON("%ld", long)
DEF_BASETYPE_TO_JSON("%lu", unsigned long)
DEF_BASETYPE_TO_JSON("%lld", long long)
DEF_BASETYPE_TO_JSON("%llu", unsigned long long)
DEF_BASETYPE_TO_JSON("%f", double)
DEF_BASETYPE_TO_JSON("%Lf", long double)


inline
void to_json_value_help2(std::string &out, std::string str) {
	json_encode(str);
	out.append("\"");
	out.append(str);
	out.append("\"");
}

inline
void to_json_value_help2(std::string &out, char const * str) {
	to_json_value_help2(out, std::string(str));
}

template <typename T>
inline 
void to_json_value_help2(std::string & out, T const & val)
{
    return to_json_value_help(out, val);
}

template <typename t>
inline
void to_json_value_help2(std::string & out, std::vector<t> const & vec) {
	out.append("[");
	for ( size_t i=0, len=vec.size();
            i<len; ++i){
	       if (i > 0) out.append(",");
	       to_json_value_help2(out, vec[i]);
	}
	out.append("]");
}



#define to_json_attr2(out, name, value) \
    do { \
        out.append("\""); \
        out.append(name); \
        out.append("\""); \
        out.append(":"); \
        to_json_value_help2(out, value); \
    } while(0)\

#define to_json_attr(out, name) \
    do { \
        to_json_attr2(out, #name, name); \
    } while(0) \

template <typename t>
inline
void to_json_value_help2(std::string & out, std::map<std::string, t> const & map) {
       out.append("{");
    size_t i=0;
    for (typeof(map.begin()) it = map.begin(), iend = map.end();
        it!=iend; ++it, ++i) {
       if (i > 0) out.append(",");
       to_json_attr2(out, it->first, it->second);
    }
   out.append("}");
}

template <typename t>
inline
void to_json_value_help2(std::string & out, std::map<char *, t> const & map) {
       out.append("{");
    size_t i=0;
    for (typeof(map.begin()) it = map.begin(), iend = map.end();
        it!=iend; ++it, ++i) {
       if (i > 0) out.append(",");
       to_json_attr2(out, it->first, it->second);
    }
   out.append("}");
}

template <typename t>
inline
void to_json_value_help2(std::string & out, std::map<char const *, t> const & map) {
       out.append("{");
    size_t i=0;
    for (typeof(map.begin()) it = map.begin(), iend = map.end();
        it!=iend; ++it, ++i) {
       if (i > 0) out.append(",");
       to_json_attr2(out, it->first, it->second);
    }
   out.append("}");
}


#define BASE_TYPE_MAP_TO_JSON(t1) \
template <typename t> \
inline \
void to_json_value_help2(std::string & out, std::map<t1, t> const & map) { \
       out.append("{"); \
    size_t i=0; \
    for (typeof(map.begin()) it = map.begin(), iend = map.end(); \
        it!=iend; ++it, ++i) { \
       if (i > 0) out.append(","); \
       std::string key; \
       to_json_value(key, it->first); \
       to_json_attr2(out, key, it->second); \
    }  \
   out.append("}"); \
} \

BASE_TYPE_MAP_TO_JSON(char)
BASE_TYPE_MAP_TO_JSON(int)
BASE_TYPE_MAP_TO_JSON(short int)
BASE_TYPE_MAP_TO_JSON(long)
BASE_TYPE_MAP_TO_JSON(long long)

BASE_TYPE_MAP_TO_JSON(unsigned char)
BASE_TYPE_MAP_TO_JSON(unsigned int)
BASE_TYPE_MAP_TO_JSON(unsigned short int)
BASE_TYPE_MAP_TO_JSON(unsigned long)
BASE_TYPE_MAP_TO_JSON(unsigned long long)


template <typename t>
inline
std::string to_json(t const & v) {
    std::string out;
    return to_json_value_help2(v);
}

template <typename t>
inline
void to_json_value(std::string & out, t const &v) {
    to_json_value_help2(out, v);
}

#define CONET_TO_JSON_MEM_CALL(r, out, i, name) \
    BOOST_PP_EXPR_IF(i, {out.append(",");}) to_json_attr2(out, BOOST_PP_STRINGIZE(name), v.name);

#define DEF_TO_JSON(type, param) DEF_TO_JSON_IMPL(type, BOOST_PP_VARIADIC_TO_SEQ param)

#define DEF_TO_JSON_IMPL(type, seq_param)\
inline  \
void to_json_value_help(std::string & out, type const & v) { \
    out.append("{"); \
    BOOST_PP_SEQ_FOR_EACH_I(CONET_TO_JSON_MEM_CALL, out, seq_param) \
    out.append("}"); \
} \
\

#define DEF_TO_JSON_MEM(type, param) DEF_TO_JSON_MEM_IMPL(type, BOOST_PP_VARIADIC_TO_SEQ param)

#define DEF_TO_JSON_MEM_IMPL(type, seq_param) \
friend \
inline \
void to_json_value_help(std::string & out, type const &v) { \
    out.append("{"); \
    BOOST_PP_SEQ_FOR_EACH_I(CONET_TO_JSON_MEM_CALL, out, seq_param) \
    out.append("}"); \
} \


#endif
