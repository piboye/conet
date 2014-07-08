/*
 * =====================================================================================
 *
 *       Filename:  ip_list.h
 *
 *    Description:  
 *
 *        Version:  1.0
 *        Created:  2014年05月18日 11时17分18秒
 *       Revision:  none
 *       Compiler:  gcc
 *
 *         Author:  piboye
 *   Organization:  
 *
 * =====================================================================================
 */
#ifndef __IP_LIST_H_INC__
#define __IP_LIST_H_INC__

#include <string>
#include <vector>
#include <string.h>
#include <stdlib.h>


struct ip_port_t
{
    std::string ip;
    int port;
};


static
inline 
void parse_ip_list(std::string txt, std::vector<ip_port_t> * list)
{

    char * start = (char *) txt.c_str();
    ip_port_t ip_port;
    char * p  = NULL; 
    p = strtok(start, ";");
    while(p)
    {
       char * p2 = p;
       while ( *p2 != ':' && *p2 != 0) ++p2;
       if (*p2 == ':') {
           ip_port.ip.assign(p, p2);
           ip_port.port = atoi(p2+1);
           list->push_back(ip_port);
       }
       p = strtok(NULL, ";");
    }
    return ;
}

#endif /* end of include guard */
