/*
 * =====================================================================================
 *
 *       Filename:  json_to_test.cpp
 *
 *    Description:  
 *
 *        Version:  1.0
 *        Created:  07/06/2015 01:32:32 AM
 *       Revision:  none
 *       Compiler:  gcc
 *
 *         Author:  piboye
 *   Organization:  
 *
 * =====================================================================================
 */
#include <stdlib.h>

#include "json_to.h"
#include "gtest/gtest.h"
#include <vector>

namespace 
{

struct A
{
    int i;
    std::vector<int> j;
    struct B
    {
        int k;
        int m;
        std::string name;
        DEF_JSON_TO_MEMBER(B, (k, m, name))
    };

    B b;
};

DEF_JSON_TO(A, (i,j, b))
}




TEST(json_to, test)
{
    std::string json_txt="{\"i\":3, \"j\":[1,2,3,4], \"b\":{\"k\":5, \"m\":6, \"name\":\"abc\"}}";
    A a;
    json_to(json_txt, &a);
    
    ASSERT_EQ(3, a.i);
    ASSERT_EQ(4, (int)a.j.size());
    ASSERT_EQ(1, a.j[0]);

    ASSERT_EQ(5, a.b.k);
    ASSERT_EQ(6, a.b.m);

    ASSERT_STREQ("abc", a.b.name.c_str());
}
