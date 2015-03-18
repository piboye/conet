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

class A
{
    public: 
        int f(int i, int j)
        {
            printf("hello, %d, %d\n", i, j);
            return 0;
        }
};

using namespace conet;

int main(int argc, char const* argv[])
{
    A a;
    int (*f)(int, int) = BindThis(a, A::f);
    f(1, 2);
    return 0;
}

