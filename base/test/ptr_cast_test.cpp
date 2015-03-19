/*
 * =====================================================================================
 *
 *       Filename:  fn_ptr_cast_test.cpp
 *
 *    Description:  
 *
 *        Version:  1.0
 *        Created:  2014年11月02日 22时24分20秒
 *       Revision:  none
 *       Compiler:  gcc
 *
 *         Author:  YOUR NAME (), 
 *   Organization:  
 *
 * =====================================================================================
 */
#include <stdlib.h>
#include "ptr_cast.h"
#include <stdio.h>
#include "thirdparty/gtest/gtest.h"

class A
{
public:
    int f(int b)
    {
        return b;
    }
};

typedef int (*f_t)(void *arg, int);

TEST(ptr_cast, test)
{
    f_t f;

    f = conet::ptr_cast<f_t>(&A::f);
    A a;

    int b = f(&a, 100);

    ASSERT_EQ(b, 100); 
}
