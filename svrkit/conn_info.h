/*
 * =====================================================================================
 *
 *       Filename:  conn_info.h
 *
 *    Description:  
 *
 *        Version:  1.0
 *        Created:  2014年11月02日 20时34分19秒
 *       Revision:  none
 *       Compiler:  gcc
 *
 *         Author:  piboye
 *   Organization:  
 *
 * =====================================================================================
 */
#ifndef __CONET_CONN_INFO_H__
#define __CONET_CONN_INFO_H__

#include <netinet/in.h>
#include <string.h>

namespace conet
{

struct coroutine_t;

struct conn_info_t
{
    void * server;
    uint32_t ip;
    int port;
    int fd;
    struct sockaddr_in addr;
    coroutine_t *co;
    void *extend;

    conn_info_t()
    {
        server = NULL;
        fd = -1;
        co = NULL;
        extend = NULL;
        memset(&addr, 0, sizeof(addr));
    }
};

}

#endif /* end of include guard */
