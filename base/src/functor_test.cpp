/*
 * =====================================================================================
 *
 *       Filename:  functor_test.cpp
 *
 *    Description:  
 *
 *        Version:  1.0
 *        Created:  2014年12月18日 05时43分03秒
 *       Revision:  none
 *       Compiler:  gcc
 *
 *         Author:  piboye
 *   Organization:  
 *
 * =====================================================================================
 */
#include "functor.h"
#include <stdlib.h>
#include <stdio.h>
#include "thirdparty/gtest/gtest.h"

using namespace conet;
TEST(functor, new_func)
{
    int c=1, b=2;
    Closure<int,int> *a = NewFunc(int, (int k), (c, b),
    {
            printf("hello k:%d, c:%d, d:%d\n", k,  c, b);
            return k+c+b;
    });
    ASSERT_EQ(6, a->Run(3));
    delete a;

    Closure<int> *a2 = NewFunc(int,(),
    {
            printf("hello \n");
            return 4;
    });
    ASSERT_EQ(4, a2->Run());
    delete a2;
}

