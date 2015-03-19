/*
 * =====================================================================================
 *
 *       Filename:  to_xml_test.cpp
 *
 *    Description:  
 *
 *        Version:  1.0
 *        Created:  03/19/2015 07:17:07 PM
 *       Revision:  none
 *       Compiler:  gcc
 *
 *         Author:  piboye
 *   Organization:  
 *
 * =====================================================================================
 */
#include <stdlib.h>
#include "to_xml.h"
#include "gtest/gtest.h"

struct A
{
    int i;
    int j;
    struct B
    {
        int k;
        int m;
        DEF_TO_XML_MEM_WITH_ATTR("A", (k), (m))
    };

    B b;
};

DEF_TO_XML(A, (i,j, b))

TEST(to_xml, serialize)
{
    A a;
    a.i=3;
    a.j=4;
    a.b.k= 5;
    a.b.m= 6;
    std::string out;
    to_xml_value(out, a);
    printf("%s", out.c_str());
}
