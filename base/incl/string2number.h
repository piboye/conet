/*
 * =====================================================================================
 *
 *       Filename:  string2number.h
 *
 *    Description:  
 *
 *        Version:  1.0
 *        Created:  2014年10月12日 05时49分33秒
 *       Revision:  none
 *       Compiler:  gcc
 *
 *         Author:  piboyeliu
 *   Organization:  
 *
 * =====================================================================================
 */

#ifndef STRING2NUMBER_H_INC
#define STRING2NUMBER_H_INC

namespace conet
{
    template <typename IT>
    int string2number(char const * src, IT *i)
    {
        long long n = 0;
        n = strtoll(src, NULL, 10);
        *i = (typeof(*i)) n;
        return 0;
    }

    template <typename IT>
    int string2number(std::string const &src, IT *i)
    {
        return string2number(src.c_str(), i);
    }

    template <>
    int string2number<uint64_t>(char const * src, uint64_t *i)
    {
        unsigned long long n = 0;
        n = strtoull(src, NULL, 10);
        *i = n;
        return 0;
    }

    template <>
    int string2number<unsigned long long>(char const * src, unsigned long long *i)
    {
        unsigned long long n = 0;
        n = strtoull(src, NULL, 10);
        *i = n;
        return 0;
    }

    template <>
    int string2number<double>(char const * src, double *i)
    {
        *i = strtod(src, NULL);
        return 0;
    }

    template <>
    int string2number<float>(char const * src, float *i)
    {
        *i = strtof(src, NULL);
        return 0;
    }

    template <typename IT>
    std::string number2string(IT i)
    {
        int64_t n = i;        
        std::string out;
        out.resize(20);
        size_t len = 0;
        len = snprintf((char *)out.c_str(), out.size(), "%" PRIi64, n);
        out.resize(len);
        return out;
    }

    template <>
    std::string number2string<unsigned long long>(unsigned long long i)
    {
        uint64_t n = i;        
        std::string out;
        out.resize(20);
        size_t len = 0;
        len = snprintf((char *)out.c_str(), out.size(), "%" PRIu64, n);
        out.resize(len);
        return out;
    }

    template <>
    std::string number2string<uint64_t>(uint64_t i)
    {
        uint64_t n = i;        
        std::string out;
        out.resize(20);
        size_t len = 0;
        len = snprintf((char *)out.c_str(), out.size(), "%" PRIu64, n);
        out.resize(len);
        return out;
    }

    template <>
    std::string number2string<float>(float i)
    {
        double n = i;        
        std::string out;
        out.resize(40);
        size_t len = 0;
        len = snprintf((char *)out.c_str(), out.size(), "%f", n);
        out.resize(len);
        return out;
    }

    template <>
    std::string number2string<double>(double i)
    {
        double n = i;        
        std::string out;
        out.resize(40);
        size_t len = 0;
        len = snprintf((char *)out.c_str(), out.size(), "%f", n);
        out.resize(len);
        return out;
    }

}

#endif /* end of include guard */
