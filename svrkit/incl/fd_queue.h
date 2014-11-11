/*
 * =====================================================================================
 *
 *       Filename:  fd_queue.h
 *
 *    Description:  
 *
 *        Version:  1.0
 *        Created:  2014年11月11日 05时21分17秒
 *       Revision:  none
 *       Compiler:  gcc
 *
 *         Author:  YOUR NAME (), 
 *   Organization:  
 *
 * =====================================================================================
 */
#ifndef __CONET__FD_QUEUE_H__
#define __CONET__FD_QUEUE_H__

#include <stdint.h>

namespace conet
{

//一写多读是安全的
//
struct FdQueue
{

    // mmap 的 大小
    uint64_t alloc_size;

    // 最多 fd 数量
    uint64_t max_num;

    // 当前 fd 数量
    volatile uint64_t fd_num;

    // 读位置
    volatile uint64_t read_pos; 

    // 写位置
    volatile uint64_t write_pos;

    volatile int64_t fds[0];    //8字节在64机器上保证了原子读写

    //
    static FdQueue * create(uint64_t max_num);

    static void free(FdQueue *q);

    
    // 没有句柄
    bool empty();

    // 满了
    bool full();
    
    // 
    bool push_fd(int fd);

    // -1 表示没有
    int pop_fd();

};


}


#endif /* end of include guard */
