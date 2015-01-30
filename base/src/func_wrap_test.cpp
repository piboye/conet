/*
 * =====================================================================================
 *
 *       Filename:  func_wrap_test.cpp
 *
 *    Description:  
 *
 *        Version:  1.0
 *        Created:  2015年01月13日 19时59分07秒
 *       Revision:  none
 *       Compiler:  gcc
 *
 *         Author:  YOUR NAME (), 
 *   Organization:  
 *
 * =====================================================================================
 */
#include <stdlib.h>
#include "func_wrap.h"
#include <stdio.h>
#include <sys/mman.h>
#include <string.h>
#include <errno.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <fcntl.h>

extern "C" void jump_to_real_func(void);

class A
{
    public: 
        int f(int i)
        {
            printf("hello\n");
            return 0;
        }
        static int f2(void *self, int i)
        {
            return ((A*)(self))->f(i);
        }
};

using namespace conet;

int main(int argc, char const* argv[])
{
    A a;
    int (*f)(int) = BindThis(a, &A::f);
    f(1);
    return 0;
}

