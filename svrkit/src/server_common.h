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

#include "base/incl/ip_list.h"

namespace conet
{

    int is_thread_mode();

    int get_listen_fd(char const *ip, int port, int listen_fd);

    int create_listen_fd(ip_port_t const &ip_port);
}


#endif /* end of include guard */
