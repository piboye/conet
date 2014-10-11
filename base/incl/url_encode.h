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
 *         Author:  YOUR NAME (), 
 *   Organization:  
 *
 * =====================================================================================
 */

#ifndef URL_ENCODE_H
#define URL_ENCODE_H


#include <string>
namespace conet
{

void url_encode(std::string const & src, std::string * out);

void url_decode(const std::string& szToDecode, std::string *out);

}
#endif /* end of include guard */

