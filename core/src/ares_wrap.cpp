/*
 * =====================================================================================
 *
 *       Filename:  ares_wrap.cpp
 *
 *    Description:  
 *
 *        Version:  1.0
 *        Created:  03/18/2015 07:57:59 PM
 *       Revision:  none
 *       Compiler:  gcc
 *
 *         Author:  piboye
 *   Organization:  
 *
 * =====================================================================================
 */
#include "ares_wrap.h"
#include <stdlib.h>

namespace conet
{
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
                len += sizeof(in_addr);
                len += sizeof(char *);
            }
            len += sizeof(char *);
            return len;
        }

    static
        int hostent_copy(hostent * src, hostent *dst, char *p, int len)
        {
            int nlen =  strlen(src->h_name);
            bcopy(src->h_name, p, nlen);
            p[nlen] = 0;
            dst->h_name = p;
            p+=(nlen+1);

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
                p+=(nlen +1);
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
                bcopy(src->h_addr_list[i], p, sizeof(in_addr)); 
                p_addr_list[i] = p;
                p+=nlen;
            }
            p_addr_list[i] = 0;


            dst->h_aliases = p_aliases;
            dst->h_addr_list = p_addr_list;
            dst->h_addrtype = src->h_addrtype;
            dst->h_length = src->h_length;
            //dst->h_addr = p_addr_list[0];
            return 0;
        }

    static
        int hostent_copy(hostent * src, hostent *dst, std::vector<char> *buff)
        {
            int len = calc_hostent_len(src);
            buff->clear();
            buff->resize(len);

            char *p = buff->data();
            return hostent_copy(src, dst, p, len);
        }

    int AresWrap::gethostbyname2_cache(char const *name, int af, cb_ctx_t &ctx)
    {

        std::string host_name(name);
        host_name.append((char *)&af, sizeof(af));
        AUTO_VAR(it, =, m_hostent_cache.find(host_name));
        if (it != m_hostent_cache.end()) {
            uint64_t now = conet::get_tick_ms() / 1000;
            HostEnt &data = it->second;
            if ((data.data_time + 5 < now) && ((data.data_time + 10)<=now)) 
            {
                list_del_init(&data.link);
                list_add(&data.link, &m_prefetch_queue);
                m_prefetch_wait.wakeup_all();
            }
            if (data.data_time + 10 > now) {
                // 缓存 10 秒
                hostent_copy(&data.host, &ctx.host, &ctx.host_buff);
                ctx.status = ARES_SUCCESS;
                return 0;
            }
        }
        int ret = gethostbyname2_raw(name, af, ctx);
        if (0 == ret) {
            uint64_t now = conet::get_tick_ms() / 1000;
            HostEnt &hc = m_hostent_cache[host_name];
            hc.host_name = name;
            hc.af = af;
            hostent_copy(&ctx.host, &hc.host, &hc.buffer);
            hc.data_time = now;
            list_del_init(&hc.link);
        }
        return ret;
    }

    int AresWrap::gethostbyname2_r(const char *name, int af, struct hostent *ret, char *buf, size_t buflen,
            struct hostent **result, int *h_errnop)
    {
        CO_DEF_STATIC_VAR0(cb_ctx_t, ctx);

        ctx.host_name = name;

        int ret2 = gethostbyname2_cache(name, af, ctx);

        if (ret2 == 0 && ctx.status == ARES_SUCCESS) {
            if (h_errnop) {
                *h_errnop =  0;
            }
            int need_host_len = calc_hostent_len(&ctx.host);
            if (need_host_len > (int)buflen) {
                return ERANGE;
            }
            hostent_copy(&ctx.host, ret, buf, buflen);
            if (result) {
                *result = ret;
            }
            return 0;
        }
        if (h_errnop) {
            switch(ctx.status)
            {
                case ARES_ENOTFOUND :
                    *h_errnop = HOST_NOT_FOUND;
                    break;
                case ARES_ENOTIMP:
                case ARES_EBADNAME:
                    *h_errnop = NO_RECOVERY;
                    break;
                default:
                    *h_errnop = NO_RECOVERY;
                    break;
            }
        }
        return -1;
    }

    int AresWrap::init()
    {
        int ret = 0;

        coroutine_t *co = conet::alloc_coroutine(&do_gethostname_prefetch,  this);
        conet::set_auto_delete(co);
        conet::resume(co);

        m_prefetch_co = co;

        ret = ares_init(&m_channel);
        if (ret != ARES_SUCCESS) {
            return -1;
        }

        struct ares_options options;
        memset(&options, 0, sizeof(options));
        options.flags = ARES_FLAG_NOCHECKRESP;
        options.sock_state_cb = ares_sockstate_cb;
        options.sock_state_cb_data = this;

        /* We do the call to ares_init_option for caller. */
        ret = ares_init_options(&m_channel,
                &options,
                ARES_OPT_FLAGS  |
                ARES_OPT_SOCK_STATE_CB);
        if(ret != ARES_SUCCESS)
        {
            return -2;
        }

        return 0;
    }

    void AresWrap::ares_sockstate_cb(void *data, int fd, int read, int write)
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
            }
            self->m_tasks.erase(fd);
        }
    }

    //static
    void AresWrap::gethostbyname_cb(void *arg, int status, int timeouts, struct hostent *host)
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


    //static
    int AresWrap::do_gethostname_prefetch(void *arg)
    {
        AresWrap * self = (AresWrap *)(arg);
        cb_ctx_t ctx;
        while(!self->m_stop_flag)
        {
            if (list_empty(&self->m_prefetch_queue))
            {
                self->m_prefetch_wait.wait_on();
                continue;
            }

            HostEnt *data=NULL, *next=NULL;
            list_for_each_entry_safe(data, next, &self->m_prefetch_queue, link) 
            {
                int ret = self->gethostbyname2_raw(data->host_name.c_str(), data->af, ctx);
                if (0 == ret) {
                    uint64_t now = conet::get_tick_ms() / 1000;
                    hostent_copy(&ctx.host, &data->host, &data->buffer);
                    data->data_time = now;
                }
                list_del_init(&data->link);
            }
        }
        return 0;
    }

    int AresWrap::gethostbyname2_raw(char const *name, int af, cb_ctx_t &ctx)
    {
        ctx.finished = 0;
        ctx.co = NULL;
        //本地 host 会直接回调
        ares_gethostbyname(m_channel, name, af, gethostbyname_cb, &ctx);

        if (!ctx.finished) {
            ctx.co = CO_SELF();
            ++m_query_cnt;
            conet::yield();
            --m_query_cnt;
        }

        if (ctx.status != ARES_SUCCESS) {
            return -1;
        }
        return 0;

    }

    hostent* AresWrap::gethostbyname2(char const *name, int af)
    {

        CO_DEF_STATIC_VAR0(cb_ctx_t, ctx);

        ctx.host_name = name;

        int ret = gethostbyname2_cache(name, af, ctx);

        if (ret != 0 || ctx.status != ARES_SUCCESS) {
            return NULL;
        }

        return &ctx.host;
    }
}
