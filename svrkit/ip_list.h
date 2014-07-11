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
#include "bobhash.h"


struct ip_port_t
{
    std::string ip;
    int port;
};

inline
bool operator<(ip_port_t const &lval, ip_port_t const &rval )
{
    int i = lval.ip.compare(rval.ip); 
    if (i>0) return false;
    if (i<0) return true;
    return lval.port < rval.port;
}

inline
bool operator==(ip_port_t const &lval, ip_port_t const &rval )
{
    int i = lval.ip.compare(rval.ip); 
    if (i) return false;
    return lval.port == rval.port;
}

size_t hash_value(ip_port_t const &val)
{
    size_t seed = 0;    
    seed = bob_hash(val.ip.c_str(), val.ip.size(), 251);
    size_t seed2 = bob_hash(&val.port, sizeof(val.port), 251);
    seed = 3*seed + 23*seed2;
    return seed;   
}


static 
void get_ip_port_str(char const *ip, int port, std::string *key) 
{
    char buf[20];
    int len =0;
    len = snprintf(buf, sizeof(buf), "%s:%d", ip, port);
    if (len >= (int) sizeof(buf)) len = sizeof(buf)-1;
    buf[len] = 0;
    *key = buf;
}

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
