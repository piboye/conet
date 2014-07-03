/*
*
 * =====================================================================================
 *
 *       Filename:  aio_test.cpp
 *
 *    Description:  
 *
 *        Version:  1.0
 *        Created:  2014年06月26日 22时51分50秒
 *       Revision:  none
 *       Compiler:  gcc
 *
 *         Author:  YOUR NAME (), 
 *   Organization:  
 *
 * =====================================================================================
 */
#include <stdlib.h>
#include <stdio.h>
#include <sys/types.h>
#include <fcntl.h>
#include <unistd.h>

#include "conet_all.h"
#include "thirdparty/gflags/gflags.h"

DEFINE_string(out_file, "2.txt", "output file name");

int proc(void *arg)
{

    conet::enable_sys_hook();
    
    int ret = 0;
    FILE *fp = fopen(FLAGS_out_file.c_str(), "a+");
    //ret = write(fileno(fp), "hello\n",6); 
    fprintf(stderr, "pos:%d, out:%d\n", (int)(ftell(fp)), ret);
    fputs("hello:\n", fp);
    fprintf(stderr, "pos:%d, out:%d\n", (int)(ftell(fp)), ret);
    return -1;
}

int main(int argc, char * argv[])
{
    google::ParseCommandLineFlags(&argc, &argv, false); 
    conet::coroutine_t *co = conet::alloc_coroutine(proc, NULL); 
    conet::resume(co);
    while (conet::get_epoll_pend_task_num() >0) {
        conet::dispatch_one();
    }
    return 0;
}
