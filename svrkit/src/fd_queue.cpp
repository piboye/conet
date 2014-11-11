/*
 * =====================================================================================
 *
 *       Filename:  fd_queue.cpp
 *
 *    Description:  
 *
 *        Version:  1.0
 *        Created:  2014年11月11日 05时32分56秒
 *       Revision:  none
 *       Compiler:  gcc
 *
 *         Author:  YOUR NAME (), 
 *   Organization:  
 *
 * =====================================================================================
 */
#include <stdlib.h>
#include <sys/mman.h>
#include <string.h>
#include "glog/logging.h"

#include "fd_queue.h"

namespace conet
{
    FdQueue * FdQueue::create(uint64_t max_num)
    {
        max_num += 100;

        int size = sizeof(FdQueue) + max_num * sizeof(uint64_t);


        // MAP_SHARED  为了多进程共享
        FdQueue * q = (FdQueue *)  mmap(NULL, size, PROT_READ| PROT_WRITE, MAP_SHARED | MAP_ANONYMOUS, -1, 0); 

        q->alloc_size = (uint64_t) size;

        q->max_num = max_num;
        q->fd_num = 0;
        q->read_pos = 0;
        q->write_pos = 0;
        
        memset((void *)q->fds, 0, sizeof(uint64_t)* max_num);

        return q;
    }

    bool FdQueue::push_fd(int fd)
    {
        if (fd <0)  {
            LOG(ERROR)<<"error [fd:"<<fd<<"]";
            return false;
        }

        uint64_t max_num = this->max_num;
        uint64_t wpos = write_pos;

        if ( full())
        { 
            return false;
        }

        uint64_t wpos_new = 0;

        do 
        {
            wpos = write_pos;

            this->fds[wpos] = (int64_t) fd;

            wpos_new = (wpos+1)%max_num;

        } while(!__sync_bool_compare_and_swap(&this->write_pos, wpos, wpos_new));

        return true;
    }

    bool FdQueue::empty()
    {
        return this->read_pos == this->write_pos;
    }

    bool FdQueue::full() 
    {
        return  ((max_num + write_pos - read_pos)%max_num) >= (max_num -100);
    }


    int FdQueue::pop_fd()
    {
        uint64_t max_num = this->max_num;

        uint64_t rpos =  0;
        uint64_t wpos = 0;

        int64_t fd = 0;

        uint64_t rpos_new = 0;
        do 
        {
            rpos = read_pos;
            wpos = write_pos;

            if ( rpos == wpos)
            { 
                //为空
                return -1;
            }

            fd = this->fds[rpos];

            rpos_new = (rpos+1)%max_num;

        } while(!__sync_bool_compare_and_swap(&this->read_pos, rpos, rpos_new));

        return (int) fd;
    }

    void FdQueue::free(FdQueue *q)
    {
        munmap(q, q->alloc_size);
    }
}

