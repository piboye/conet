#ifndef LOG_FORMAT_H_V5QBZBO4
#define LOG_FORMAT_H_V5QBZBO4
#include <string>
#include <vector>
#include <list>
#include <map>

#ifndef BOOST_PP_VARIADICS
//  gcc 必须定义这个， 不然会定义为 0, 导致 BOOST_PP_VARIADIC_TO_SEQ 用不了
#define BOOST_PP_VARIADICS 2 
#endif 

#include "string2number.h"
#include <boost/preprocessor.hpp>
#include <boost/preprocessor/variadic/to_seq.hpp>
#include <boost/preprocessor/seq/for_each.hpp>
#include <boost/vmd/is_seq.hpp>
#include <boost/vmd/is_tuple.hpp>
#include <boost/preprocessor/stringize.hpp>

/*
    template <typename IT>
    inline
    std::string &log_format(std::string & data, IT i)
    {
        int64_t n = i;
        size_t org_size = data.size();
        data.resize(20);
        size_t len = 0;
        len = snprintf((char *)data.c_str()+org_size, 20, "%" PRIi64, n);
        data.resize(len+org_size);
        return data;
    }
*/

#define DEFINE_LOG_FORMAT_FOR_BASE(type, type2, fmt) \
    inline \
    std::string &log_format(std::string & out, type i) \
    { \
        type2 n = i; \
        size_t org_size = out.size(); \
        out.resize(org_size+30); \
        size_t len = 0; \
        len = snprintf((char *)out.c_str()+org_size, 30, "%" fmt, n); \
        out.resize(len+org_size); \
        return out; \
    }


DEFINE_LOG_FORMAT_FOR_BASE(int64_t, int64_t, PRIi64)
DEFINE_LOG_FORMAT_FOR_BASE(uint64_t, int64_t, PRIu64)
DEFINE_LOG_FORMAT_FOR_BASE(int32_t, int32_t, "d")
DEFINE_LOG_FORMAT_FOR_BASE(uint32_t, uint32_t, "u")
DEFINE_LOG_FORMAT_FOR_BASE(float, double, "f")
DEFINE_LOG_FORMAT_FOR_BASE(double, double, "f")

    inline
    std::string & log_format(std::string & out, bool i)
    {
        if (i) {
            out.append("true");
        } else {
            out.append("false");
        }
        return out;
    }

    inline
    std::string &log_format(std::string & out, std::string const & data)
    {
        out.append(data);
        return out;
    }

    inline
    std::string &log_format(std::string & out, char const * data)
    {
        out.append(data);
        return out;
    }


    template<typename T>
    inline
    std::string &log_format(std::string & out, std::vector<T> const &data)
    {
        out.append("[");
        for (size_t i = 0; i<data.size(); ++i)
        {
            if (i > 0) out.append(",");
            log_format(out, data[i]);
        }
        out.append("]");
        return out;
    }

    template<typename T>
    inline
    std::string &log_format(std::string & out, std::map<std::string, T> const &data)
    {
        out.append("{");
        __typeof__(data.begin()) it = data.begin();
        int i = 0;
        for (; it!=data.end(); ++it)
        {
            if (i >0) out.append(",");
            out.append("\"");
            log_format(out, it->first);
            out.append("\":");
            log_format(out, it->second);
            ++i;
        }
        out.append("}");
        return out;
    }
    

#define LOG_FORMAT_NAME_ELEM_IMPL(r, out, I, elem)  \
    { \
        out.append(" [" BOOST_PP_STRINGIZE(elem) "=");  \
        log_format(out, elem); \
        out.append("]"); \
    }


// BOOST_PP_SEQ_FOR_EACH 在一个宏中 用多次会失败，
// 因为 BOOST_PP_SEQ_FOR_EACH 不是可重入的， 也可能是gcc 版本太低

#define LOG_FORMAT_ELEM_IMPL(r, out, elem) \
    { \
        BOOST_PP_IIF(BOOST_VMD_IS_TUPLE(elem), \
          BOOST_PP_SEQ_FOR_EACH_I(LOG_FORMAT_NAME_ELEM_IMPL, out, BOOST_PP_VARIADIC_TO_SEQ elem), \
            log_format(out, elem)); \
    }



#define LOG_FORMAT(__log_format_out_obj__, ...) \
    do { \
        BOOST_PP_SEQ_FOR_EACH(LOG_FORMAT_ELEM_IMPL, __log_format_out_obj__, BOOST_PP_VARIADIC_TO_SEQ(__VA_ARGS__)) \
    } while(0)

#define LOG_FORMAT_STRUCT_ELEM_IMPL(r, out, I, elem) \
    __log_format_out_txt__.append( \
        BOOST_PP_IF(I, ", ", "") BOOST_PP_STRINGIZE(elem) ":"); \
        log_format(__log_format_out_txt__, out.elem); \




#define DEF_STRUCT_LOG(struct_, members) \
    inline \
    std::string &log_format(std::string & __log_format_out_txt__, \
    struct_ const & __log_format_out_obj__) \
    {  \
        __log_format_out_txt__.append("{"); \
        BOOST_PP_SEQ_FOR_EACH_I(LOG_FORMAT_STRUCT_ELEM_IMPL, __log_format_out_obj__, BOOST_PP_VARIADIC_TO_SEQ members) \
        __log_format_out_txt__.append("}"); \
        return __log_format_out_txt__; \
    }


#endif /* end of include guard: LOG_FORMAT_H_V5QBZBO4 */
