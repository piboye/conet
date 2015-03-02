/*
 * =====================================================================================
 *
 *       Filename:  server_common.h
 *
 *    Description:  
 *
 *        Version:  1.0
 *        Created:  2014年12月12日 07时38分13秒
 *       Revision:  none
 *       Compiler:  gcc
 *
 *         Author:  piboye
 *   Organization:  
 *
 * =====================================================================================
 */

#ifndef __CONET_SERVER_COMMON_H__
#define __CONET_SERVER_COMMON_H__

#include "../base/ip_list.h"

namespace conet
{

    int is_thread_mode();

    int get_listen_fd(char const *ip, int port, int listen_fd);

    int create_listen_fd(ip_port_t const &ip_port);

    int set_server_stop();
    int get_server_stop_flag();

    typedef void server_fini_func_t(void);
    int registry_server_fini_func(server_fini_func_t *func);
    int call_server_fini_func();

    int get_listen_fd_from_pool(char const *ip, int port);

#define REG_SERVER_FININSH(func) \
    static int CONET_MACRO_CONCAT(g_registry_fini_, __LINE__) = conet::registry_server_fini_func(func)
}


#endif /* end of include guard */
