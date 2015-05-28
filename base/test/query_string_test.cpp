/*
 * =====================================================================================
 *
 *       Filename:  query_string_test.cpp
 *
 *    Description:  
 *
 *        Version:  1.0
 *        Created:  2014年07月28日 23时20分18秒
 *       Revision:  none
 *       Compiler:  gcc
 *
 *         Author:  YOUR NAME (), 
 *   Organization:  
 *
 * =====================================================================================
 */
#include <stdlib.h>
#include <stdlib.h>
#include "query_string.h"
#include "thirdparty/gtest/gtest.h"


using namespace conet;

TEST(query_string, parse)
{
    std::map<std::string, std::string> params;
    std::string txt = "a=3&b=f4+"; 
    int ret = 0;
    ret = parse_query_string(txt.c_str(), txt.size(), &params);
    ASSERT_EQ(0, ret);
    ASSERT_STREQ("3", params["a"].c_str());
    ASSERT_STREQ("f4+", params["b"].c_str());
}

TEST(query_string, to_json)
{
    std::string txt = "a=3&b=f4"; 
    int ret = 0;
    Json::Value root;
    ret = query_string_to_json(txt.c_str(), txt.size(), &root);
    ASSERT_EQ(0, ret);
    printf("json:%s\n",  root.toStyledString().c_str());
}
