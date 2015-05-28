/*
 * =====================================================================================
 *
 *       Filename:  url_encode.h
 *
 *    Description:  
 *
 *        Version:  1.0
 *        Created:  05/28/2015 01:37:21 AM
 *       Revision:  none
 *       Compiler:  gcc
 *
 *         Author:  piboyeliu
 *   Organization:  
 *
 * =====================================================================================
 */
#include <stdint.h>
#include <string>

namespace conet
{

namespace 
{

static
inline
int hex_char(char ch)
{
    switch(ch)
    {
        case '0': return 0;
        case '1': return 1;
        case '2': return 2;
        case '3': return 3;
        case '4': return 4;
        case '5': return 5;
        case '6': return 6;
        case '7': return 7;
        case '8': return 8;
        case '9': return 9;
        case 'a': return 10;
        case 'b': return 11;
        case 'c': return 12;
        case 'd': return 13;
        case 'e': return 14;
        case 'f': return 15;
        case 'A': return 10;
        case 'B': return 11;
        case 'C': return 12;
        case 'D': return 13;
        case 'E': return 14;
        case 'F': return 15;
        default: return -1;
    }
    return -1;
}

}

int url_decode(char const *buf, size_t len, std::string *out) 
{

    out->resize(len);

    char * pout= (char *)out->c_str();
    char *pstart  = pout;

    for( size_t i=0; i < len; ++i)
    {
        char ch = buf[i]; 

        if (ch == '%' && i+2<len)
        {
            int v = hex_char(buf[i+1]); 
            if (v<0) {
                *pout++ = ch;
                continue;
            }
            int v2 = hex_char(buf[i+2]); 
            if (v2<0) {
                *pout++ = ch;
                continue;
            }
            *pout++ = (char)((v<<4)+v2);
            i+=2;
        } else {
            *pout++ = ch;
        }
    }

    out->resize(pout - pstart);
    return 0;
}

int url_decode(const std::string& src, std::string *out)
{
    return url_decode(src.c_str(), src.size(), out);
}

void url_encode(char const * src ,  size_t len, std::string * out)
{
    std::string &dst = *out;
    char hex[] = "0123456789ABCDEF";
    for (size_t i = 0; i < len; ++i)
    {
        unsigned char cc = src[i];
        if (isascii(cc))
        {
            if (cc == ' ')
            {
                dst += "%20";
            }
            else
                dst += cc;
        }
        else
        {
            unsigned char c = static_cast<unsigned char>(src[i]);
            dst += '%';
            dst += hex[c / 16];
            dst += hex[c % 16];
        }
    }
}

void url_encode(std::string const & src, std::string * out)
{
    url_encode(src.c_str(), src.size(), out); 
}

}
