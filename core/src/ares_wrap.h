/*
 * =====================================================================================
 *
 *       Filename:  ares_wrap.h
 *
 *    Description:  
 *
 *        Version:  1.0
 *        Created:  2014年12月15日 00时36分08秒
 *       Revision:  none
 *       Compiler:  gcc
 *
 *         Author:  piboye
 *   Organization:  
 *
 * =====================================================================================
 */

#ifndef __CONET_ARES_WRAP_H__
#define __CONET_ARES_WRAP_H__

#include "thirdparty/c-ares/ares.h"
#include "coroutine.h"
#include "base/incl/auto_var.h"
#include "base/incl/ptr_cast.h"
#include <netdb.h>
#include <vector>
#include <map>
#include <string>
#include "timewheel.h"
#include "thirdparty/glog/logging.h"

namespace conet
{

class AresWrap
{
public:
    ares_channel m_channel;  
    coroutine_t *m_main_co;
    int m_stop_flag;
    int m_query_cnt;

    int init()
    {
        int ret = 0;
        ret = ares_init(&m_channel);
        if (ret != ARES_SUCCESS) {
            return -1;
        }
        m_query_cnt = 0;

        struct ares_options options;
        memset(&options, 0, sizeof(options));
        //options.flags = ARES_FLAG_NOCHECKRESP;
        options.sock_state_cb = ares_sockstate_cb;
        options.sock_state_cb_data = this;
        //options.flags = ARES_FLAG_USEVC;

        /* We do the call to ares_init_option for caller. */
        ret = ares_init_options(&m_channel,
                &options,
                //ARES_OPT_FLAGS  |
                 ARES_OPT_SOCK_STATE_CB);
        if(ret != ARES_SUCCESS)
        {
            return -2;
        }

        m_main_co = NULL;
        /* 
        m_main_co = conet::alloc_coroutine(conet::ptr_cast<co_main_func_t>(&AresWrap::process), this); 
        conet::resume(m_main_co);
        */

        return 0;
    }


    struct cb_ctx_t
    {
        char const *host_name;
        coroutine_t *co;
        int status;
        hostent host;
        std::vector<char> host_buff;
        int finished;
    };

    struct HostEnt
    {
        hostent host;
        std::vector<char> buffer;
    };

    std::map<std::string,  HostEnt> m_hostent_cache;


    struct task_t
    {
        AresWrap * ares;
        int fd;
        int stop_flag;
        conet::coroutine_t * co;
        int events;

        int main_proc()
        {
            int ret = 0;
            while (!this->stop_flag)
            {
                pollfd fds;
                fds.fd = this->fd;
                fds.revents= 0;
                fds.events = this->events;
                ret = poll(&fds, 1, 1000); 
                if (ret == 0) {
                    // timeout
                    ares_process_fd(this->ares->m_channel, ARES_SOCKET_BAD, ARES_SOCKET_BAD);
                    continue;
                }
                if (ret <0 ) {
                    ares_process_fd(this->ares->m_channel, 
                            this->fd,
                            this->fd
                            );
                    continue;
                }
                ares_process_fd(this->ares->m_channel, 
                        (fds.revents & POLLIN)? this->fd: ARES_SOCKET_BAD,
                        (fds.revents & POLLOUT)? this->fd: ARES_SOCKET_BAD
                        );
            }
            delete this;
            return 0;
        }

    };

    std::map<int, task_t*> m_tasks;

    static 
    void ares_sockstate_cb(void *data, int fd, int read, int write)
    {
        AresWrap * self = (AresWrap *)(data);
        
        task_t *task = NULL;
        AUTO_VAR(it ,=, self->m_tasks.find(fd));
        if (it != self->m_tasks.end()) {
           task = it->second; 
        }
        if (read || write) 
        {
            if (!task) 
            {// new task
                task = new task_t();
                task->ares = self;
                task->fd = fd;
                task->stop_flag = 0;
                task->events = (read?POLLIN:0) |(write?POLLOUT:0);
                task->co = conet::alloc_coroutine(conet::ptr_cast<conet::co_main_func_t>(&task_t::main_proc), task);
                conet::set_auto_delete(task->co);
                self->m_tasks.insert(std::make_pair(fd, task));
                conet::resume(task->co);
                return ;
            } else {
                task->events = (read?POLLIN:0) |(write?POLLOUT:0);
            }
        } else {
            if (task) {
                task->stop_flag = 1;
                //conet::wait(task->co);
                //conet::free_coroutine(task->co);
                //delete task;
            }
            self->m_tasks.erase(fd);
        }
    }


    void * process()
    {
        int ret = 0;
        while(!m_stop_flag)
        {
            if (m_query_cnt <=0) {
                usleep(10000);
                continue;
            }
            struct timeval *tvp=NULL, tv;
            fd_set read_fds, write_fds;
            int nfds;
     
            FD_ZERO(&read_fds);
            FD_ZERO(&write_fds);
            nfds = ares_fds(m_channel, &read_fds, &write_fds);

            if(nfds == 0){
                ares_process_fd(m_channel, ARES_SOCKET_BAD, ARES_SOCKET_BAD);
                continue;
            }
            tvp = ares_timeout(m_channel, NULL, &tv);
            ret = select(nfds, &read_fds, &write_fds, NULL, tvp);
            if (ret > 0) {
                ares_process(m_channel, &read_fds, &write_fds);
            }
        }
        return NULL;
    }

    int stop()
    {
        if (m_stop_flag == 0) 
        {
            m_stop_flag = 1;
            if (m_main_co) {
                conet::wait(m_main_co);
                free_coroutine(m_main_co);
                m_main_co = NULL;
            }
        }
        return 0;
    }


    static 
    int calc_hostent_len(hostent *src)
    {
        int len = 0;
        len += strlen(src->h_name) +1;
        for (int i = 0; src->h_aliases[i]; ++i)
        {
            len += strlen(src->h_aliases[i]) +1;
            len += sizeof(char *);
        }
        len += sizeof(char *);

        for (int i = 0; src->h_addr_list[i]; ++i)
        {
            len += strlen(src->h_addr_list[i]) +1;
            len += sizeof(char *);
        }
        len += sizeof(char *);
        return len;
    }

    static
    int hostent_copy(hostent * src, hostent *dst, std::vector<char> *buff)
    {
        int len = calc_hostent_len(src);
        buff->clear();
        buff->reserve(len);

        char *p = buff->data();

        int nlen =  strlen(src->h_name);
        bcopy(src->h_name, p, nlen);
        p[nlen] = 0;
        dst->h_name = p;
        p+=nlen+1;
        
        int i = 0;

        char **p_aliases = (char **)(p);
        for (i = 0; src->h_aliases[i]; ++i)
        {
            p+=sizeof(char *);
        }
        p+=sizeof(char *);

        for (i = 0; src->h_aliases[i]; ++i)
        {
            nlen = strlen(src->h_aliases[i]);
            bcopy(src->h_aliases[i], p, nlen); 
            p[nlen] = 0;
            p_aliases[i] = p;
            p+=nlen +1;
        }
        p_aliases[i] = 0;

        char **p_addr_list = (char **)(p);
        for (i = 0; src->h_addr_list[i]; ++i)
        {
            p+=sizeof(char *);
        }
        p+=sizeof(char *);

        for (i = 0; src->h_addr_list[i]; ++i)
        {
            nlen = strlen(src->h_addr_list[i]);
            bcopy(src->h_addr_list[i], p, nlen); 
            p[nlen] = 0;
            p_aliases[i] = p;
            p+=nlen +1;
        }
        p_addr_list[i] = 0;


        dst->h_aliases = p_aliases;
        dst->h_addr_list = p_addr_list;
        dst->h_addrtype = src->h_addrtype;
        dst->h_length = src->h_length;
        dst->h_addr = p_addr_list[0];
        return 0;
    }

    static
    void gethostbyname_cb(void *arg, int status, int timeouts, struct hostent *host)
    {
        cb_ctx_t *ctx= (cb_ctx_t *) (arg);
        ctx->finished = 1;
        if ( NULL == host || status != ARES_SUCCESS) 
        {
            ctx->status = status;
            if (ctx->co) {
                conet::resume(ctx->co);
            }
            return;
        }
        hostent_copy(host, &ctx->host, &ctx->host_buff);
        ctx->status = status;

        if (ctx->co) {
            conet::resume(ctx->co);
        }
    }


    static void gethostbyname_cb_timeout(void *arg)
    {
        cb_ctx_t *ctx= (cb_ctx_t *) (arg);
        ctx->status = -1;

        conet::resume(ctx->co);
        return ;
    }

    hostent* gethostbyname(char const *name)
    {
        std::string host_name(name);
        /*
        AUTO_VAR(it, =, m_hostent_cache.find(host_name));
        if (it != m_hostent_cache.end()) {
            return &it->second.host;
        }
        */

        CO_DEF_STATIC_VAR0(cb_ctx_t, ctx);

        ctx.host_name = name;


        ctx.finished = 0;
        ctx.co = NULL;
        //本地 host 会直接回调
        ares_gethostbyname(m_channel, name, AF_INET, gethostbyname_cb, &ctx);

        if (!ctx.finished) {
            ctx.co = CO_SELF();
            ++m_query_cnt;
            conet::yield();
            --m_query_cnt;
        }

        if (ctx.status != ARES_SUCCESS) {
            return NULL;
        }

        /*
        HostEnt &hc = m_hostent_cache[host_name];
        bcopy(&ctx.host, &hc.host, sizeof(hostent));
        hc.buffer.swap(ctx.host_buff);
        return &hc.host;
        */
        return &ctx.host;
    }

    AresWrap()
    {
        m_main_co = NULL;
        m_stop_flag = 0;
        init();
    }

    ~AresWrap()
    {
        ares_destroy(m_channel);
    }

};

}

#endif /* end of include guard */

