/*
 * =====================================================================================
 *
 *       Filename:  to_json_test.cpp
 *
 *    Description:  
 *
 *        Version:  1.0
 *        Created:  03/19/2015 04:39:34 PM
 *       Revision:  none
 *       Compiler:  gcc
 *
 *         Author:  piboye
 *   Organization:  
 *
 * =====================================================================================
 */

#include "to_json.h"
#include "gtest/gtest.h"

struct A
{
    int i;
    int j;
    struct B
    {
        int k;
        int m;
        DEF_TO_JSON_MEM((k,m))
    };

    B b;
};

DEF_TO_JSON(A, (i,j, b))

TEST(to_json, test)
{
    A a;
    a.i=3;
    a.j=4;
    a.b.k = 5;
    a.b.m = 6;
    std::string out;
    to_json_value(out, a);
    printf("%s", out.c_str());
}
