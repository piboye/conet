/*
 * =====================================================================================
 *
 *       Filename:  defer_test.cpp
 *
 *    Description:  
 *
 *        Version:  1.0
 *        Created:  2015年03月09日 16时56分29秒
 *       Revision:  none
 *       Compiler:  gcc
 *
 *         Author:  YOUR NAME (), 
 *   Organization:  
 *
 * =====================================================================================
 */
#include <stdlib.h>
#include "defer.h"
#include "thirdparty/gtest/gtest.h"

void f(int *i)
{
    int j=0;
    CONET_DEFER( (i, j), {
        *i = 4;
    });
    *i=3;
}

TEST(defer, param1)
{
    int a = 1;
    f(&a);
    ASSERT_EQ(4, a);
}

