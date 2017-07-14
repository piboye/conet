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
#include "../base/auto_var.h"
#include "../base/ptr_cast.h"
#include <netdb.h>
#include <vector>
#include <map>
#include <string>
#include "timewheel.h"
#include "base/plog.h"
#include "wait_queue.h"

namespace conet
{

class AresWrap
{
public:
    ares_channel m_channel;  
    int m_stop_flag;
    int m_query_cnt;
    list_head m_prefetch_queue;
    WaitQueue m_prefetch_wait;
    coroutine_t *m_prefetch_co;


    int init();

    struct HostEnt
    {
        uint64_t data_time; // fetch seconds
        std::string host_name;
        int af;
        hostent host;
        std::vector<char> buffer;
        list_head link;
        HostEnt()
        {
            INIT_LIST_HEAD(&link);
        }
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
    void ares_sockstate_cb(void *data, int fd, int read, int write);

    int stop()
    {
        if (m_stop_flag == 0) 
        {
            m_stop_flag = 1;
        }
        return 0;
    }



    struct cb_ctx_t
    {
        char const *host_name;
        coroutine_t *co;
        int status;
        hostent host;
        std::vector<char> host_buff;

        hostent * user_host;
        char *user_host_buff;
        int user_host_len;
        int finished;
    };

    static void gethostbyname_cb(void *arg, int status, int timeouts, struct hostent *host);




    struct prefetch_task_t
    {
        AresWrap * ares;
        std::string host_name;
        int af;
    };

    static 
    int do_gethostname_prefetch(void *arg);

    int gethostbyname2_raw(char const *name, int af, cb_ctx_t &ctx);

    int gethostbyname2_cache(char const *name, int af, cb_ctx_t &ctx);


    hostent* gethostbyname(char const *name)
    {
        return this->gethostbyname2(name, AF_INET);
    }


    hostent* gethostbyname2(char const *name, int af);

    int gethostbyname_r(const char *name, struct hostent *ret, char *buf, size_t buflen,
                              struct hostent **result, int *h_errnop)
    {
        return gethostbyname2_r(name, AF_INET, ret, buf, buflen, result, h_errnop);
    }


    int gethostbyname2_r(const char *name, int af, struct hostent *ret, 
                        char *buf, size_t buflen,
                              struct hostent **result, int *h_errnop);

    AresWrap()
    {
        m_query_cnt = 0;
        m_stop_flag = 0;
        INIT_LIST_HEAD(&m_prefetch_queue);
        m_prefetch_co = NULL;
        init();
    }

    ~AresWrap()
    {
        ares_destroy(m_channel);
    }

};

}

#endif /* end of include guard */
