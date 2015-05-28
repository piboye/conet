/*
 * =====================================================================================
 *
 *       Filename:  url_encode.h
 *
 *    Description:  
 *
 *        Version:  1.0
 *        Created:  2014年07月28日 23时54分26秒
 *       Revision:  none
 *       Compiler:  gcc
 *
 *         Author:  piboyeliu
 *   Organization:  
 *
 * =====================================================================================
 */

#ifndef URL_ENCODE_H
#define URL_ENCODE_H


#include <string>
namespace conet
{

void url_encode(char const * src ,  size_t len, std::string * out);
void url_encode(std::string const & src, std::string * out);

int url_decode(const std::string& src, std::string *out);

int url_decode(char const * src, size_t len, std::string *out);

}
#endif /* end of include guard */

