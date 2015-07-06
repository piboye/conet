/*
 * =====================================================================================
 *
 *       Filename:  string_tpl_test.cpp
 *
 *    Description
 *
 *        Version:  1.0
 *        Created:  07/06/2015 02:53:46 AM
 *       Revision:  none
 *       Compiler:  gcc
 *
 *         Author:  piboye
 *   Organization:  
 *
 * =====================================================================================
 */
#include <stdlib.h>

#include "string_tpl.h"
#include "gtest/gtest.h"
#include <vector>
#include <string>
#include "string2number.h"

TEST(string_tpl, test)
{
    std::string tpl="hello:{{name}}, age:{{age}}";
    std::map<std::string, std::string> datas;
    datas["name"]="piboye";
    datas["age"] = conet::number2string(18);

    std::string out;
    std::string errmsg;

    int ret = 0;
    ret = conet::string_tpl(tpl, datas, &out, &errmsg);

    ASSERT_EQ(0, ret);

    ASSERT_STREQ("hello:piboye, age:18", out.c_str());
}
