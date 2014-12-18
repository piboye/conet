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
#include <stdlib.h>
#include <stdio.h>
#include "functor.h"


class F
{
public:
    virtual int operator()()=0;
    virtual ~F()
    {
    }
};


int main(int argc, char const* argv[])
{

    F *a = new_func_def (F, int, operator(), ()) {
            printf("hello \n");
            return 0;
    } new_func_end;
    
    (*a)();

    return 0;
}

