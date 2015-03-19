/*
 * =====================================================================================
 *
 *       Filename:  bind_this_test.cpp
 *
 *    Description:  
 *
 *        Version:  1.0
 *        Created:  2015年01月13日 19时59分07秒
 *       Revision:  none
 *       Compiler:  gcc
 *
 *         Author:  piboye
 *   Organization:  
 *
 * =====================================================================================
 */

#include "bind_this.h"

#include <stdlib.h>
#include <stdio.h>
#include <sys/mman.h>
#include <string.h>
#include <errno.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <fcntl.h>

#include "thirdparty/gtest/gtest.h"

class A
{
    public: 
        int m_i;
        int f(int i, int j)
        {
            printf("m_i:%d hello, %d, %d\n", m_i, i, j);
            return i+j;
        }
};

using namespace conet;

TEST(bind_this, test)
{
    A a;
    a.m_i = 100;
    int (*f)(int, int) = BindThis(a, A::f);
    int k = f(1, 2);
    printf("j:%d\n", k);
    ASSERT_EQ(3, k);
    free_bind_this_func(f);
}

