/*
 * =====================================================================================
 *
 *       Filename:  url_encode.cpp
 *
 *    Description:  
 *
 *        Version:  1.0
 *        Created:  2014年07月28日 23时55分14秒
 *       Revision:  none
 *       Compiler:  gcc
 *
 *         Author:  YOUR NAME (), 
 *   Organization:  
 *
 * =====================================================================================
 */
#include <stdlib.h>
#include "url_encode.h"

namespace conet
{

void url_encode(std::string const & src, std::string * out)
{
    std::string &dst = *out;
    char hex[] = "0123456789ABCDEF";
    for (size_t i = 0; i < src.size(); ++i)
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
 

int hex_char(char ch)
{
    switch(ch)
    {
#define HEX_CASE(a) case '##a##': return a;
        case '1': return 1;
        case '2': return 2;
        case '3': return 3;
        case '4': return 4;
        case '5': return 5;
        case '6': return 6;
        case '7': return 7;
        case '8': return 8;
        case '9': return 9;
        case '0': return 0;
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
    }
    return -1;
}
 
void url_decode(const std::string& src, std::string *out)
{
    url_decode(src.c_str(), src.size(), out);
}

void url_decode(char const * src, size_t len, std::string *out)
{
    std::string &result= *out;
    int hex = 0;
    for (size_t i = 0; i < len; ++i)
    {
        switch (src[i])
        {
        case '+':
            result += ' ';
            break;
        case '%':
            if (i+2< len)
            {
                int v = hex_char(src[i+1]);
                if (v<0) {
                    result.push_back('%');
                    result.push_back(src[i+1]);
                    result.push_back(src[i+2]);
                    i+=2;
                    break;
                }

                hex = v; 

                v = hex_char(src[i+2]);
                if (v<0) {
                    result.push_back('%');
                    result.push_back(src[i+1]);
                    result.push_back(src[i+2]);
                    i+=2;
                    break;
                }
                hex <<=4;
                hex += v;

                //字母和数字[0-9a-zA-Z]、一些特殊符号[$-_.+!*'(),] 、以及某些保留字[$&+,/:;=?@]
                //可以不经过编码直接用于URL
                if (!((hex >= 48 && hex <= 57) || //0-9
                    (hex >=97 && hex <= 122) ||   //a-z
                    (hex >=65 && hex <= 90) ||    //A-Z
                    //一些特殊符号及保留字[$-_.+!*'(),]  [$&+,/:;=?@]
                    hex == 0x21 || hex == 0x24 || hex == 0x26 || hex == 0x27 || hex == 0x28 || hex == 0x29
                    || hex == 0x2a || hex == 0x2b|| hex == 0x2c || hex == 0x2d || hex == 0x2e || hex == 0x2f
                    || hex == 0x3A || hex == 0x3B|| hex == 0x3D || hex == 0x3f || hex == 0x40 || hex == 0x5f
                    ))
                {
                    result.push_back(char(hex));
                    i += 2;
                }
                else result.push_back('%');
            }else {
                result.push_back('%');
            }
            break;
        default:
            result.push_back(src[i]);
            break;
        }
    }
}

}


