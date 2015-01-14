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
    void * p = mmap(0, 4096, PROT_READ| PROT_WRITE | PROT_EXEC, MAP_PRIVATE | MAP_LOCKED | MAP_ANONYMOUS, -1, 0);  
    printf("p:%p\n", p);
    FuncWrapData *d = (FuncWrapData *)(p);
    printf("d:%p\n", d);
    d->jump_func = (uint64_t)(( int(*)(int))(conet::func_wrap_pb100<int, int>));
    d->self = (uint64_t )&a;
    d->mem_func = reinterpret_cast<uint64_t>(&A::f2);
    memcpy(&(d->code), (void *)(&jump_to_real_func), 32);
    //memset(&(d->code), 0xc3, 32);
    int (*pf)(int) = (int (*)(int))(&(d->code));
    //int (*pf)(int) = (int (*)(int))(&jump_to_real_func);
    //int ret = mprotect(p, 4096, PROT_READ|PROT_EXEC);
    //printf("mprotect ret:%d, errno:%d\n", ret, errno);
    (*pf)(1);
    return 0;
}

