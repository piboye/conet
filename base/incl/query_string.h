/*
 * =====================================================================================
 *
 *       Filename:  query_string.h
 *
 *    Description:  
 *
 *        Version:  1.0
 *        Created:  2014年07月28日 23时10分40秒
 *       Revision:  none
 *       Compiler:  gcc
 *
 *         Author:  piboyeliu
 *   Organization:  
 *
 * =====================================================================================
 */

#ifndef QUERY_STRING_H
#define QUERY_STRING_H

#include <map>
#include <string>
#include "jsoncpp/value.h"
#include "jsoncpp/json.h"

namespace conet
{
    int parse_query_string(char const *buf, size_t len, std::map<std::string, std::string> *param);

    int query_string_to_json(char const *buf, size_t len, Json::Value *root);
}

#endif /* end of include guard */
