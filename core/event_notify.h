/*
 * =====================================================================================
 *
 *       Filename:  event_notify.h
 *
 *    Description:  
 *
 *        Version:  1.0
 *        Created:  2014年11月02日 02时23分18秒
 *       Revision:  none
 *       Compiler:  gcc
 *
 *         Author:  piboye
 *   Organization:  
 *
 * =====================================================================================
 */

#ifndef __CONET_EVENT_NOTIFY_H__
#define __CONET_EVENT_NOTIFY_H__
#include <sys/syscall.h>
#include <sys/eventfd.h>
#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <fcntl.h>      
#include "coroutine.h"
#include "network_hook.h"
#include "fd_ctx.h"

namespace conet
{

struct event_notify_t
{
    int fd;
    uint64_t event_num;

    typedef int (*cb_type)(void *arg, uint64_t event) ;
    cb_type cb;
    void *cb_arg;
    int stop_flag;

    coroutine_t * work_co;

    int init(cb_type cb, void *cb_arg) 
    {
        int evfd = 0; 
        evfd = eventfd(0, EFD_NONBLOCK | EFD_CLOEXEC);
        if (evfd <0) return -1;
        fd_ctx_t *ev_ctx = alloc_fd_ctx(evfd, 1);
        ev_ctx->user_flag &= ~O_NONBLOCK;

        this->fd = evfd;
        this->event_num = 0;
        this->work_co = alloc_coroutine(proc, this, 128*1024); 
        if (NULL == this->work_co) {
            close(this->fd);
            this->fd = -1;
            return -2;
        }

        this->cb = cb;
        this->cb_arg = cb_arg;
        this->stop_flag = 0;
        //set_auto_delete(this->work_co);
        conet::resume(this->work_co);
        return 0;
    }

    static int proc(void *arg)
    {
       event_notify_t *self = (event_notify_t *)(arg);

       conet::enable_sys_hook(); 

       do 
       {
           uint64_t ready = 0;
           int n = 0;
           n = read(self->fd, &ready, 8);
           if((n == 8) && ready > 0) {
               self->event_num = ready; 
               if (self->cb) {
                   self->cb(self->cb_arg, ready);
               }
           }
       } while(!self->stop_flag);

       close(self->fd);
       self->fd = -1;
       return 0; 
    }

    int notify(uint64_t num=1)
    {
       return syscall(SYS_write, this->fd, &num, sizeof(num));
    }

    int stop()
    {
        this->stop_flag = 1;
        conet::resume(this->work_co);
        int ret = wait(this->work_co); 
        conet::free_coroutine(this->work_co);
        this->work_co = NULL;
        return ret;
    }
};

}
#endif /* end of include guard */
