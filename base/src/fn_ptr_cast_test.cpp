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
#include "fn_ptr_cast.h"
#include <stdio.h>

class A
{
public:
    int f(int b)
    {
        return b;
    }
};

typedef int (*f_t)(void *arg, int);


int main(int argc, char const* argv[])
{
    f_t f;

    f = conet::fn_ptr_cast<f_t>(&A::f);
    A a;

    int b = f(&a, 100);

    printf("b:%d\n", b);
    
    return 0;
}
