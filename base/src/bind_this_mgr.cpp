/*
 * =====================================================================================
 *
 *       Filename:  bind_this_mgr.cpp
 *
 *    Description
 *
 *        Version:  1.0
 *        Created:  01/14/2015 11:37:06 AM
 *       Revision:  none
 *       Compiler:  gcc
 *
 *         Author:  piboye
 *   Organization:  
 *
 * =====================================================================================
 */
#include "list.h"
#include "bind_this_mgr.h"
#include "tls.h"

#include <sys/mman.h>
#include <string.h>
#include <stdlib.h>

extern "C" void conet_bind_this_jump_help(void);
namespace conet
{

struct BindThisMgr
{
    BindThisMgr()
    {
        INIT_LIST_HEAD(&m_free_list);
    }

    void expand()
    {
        size_t sz = 4096*4;
        void * p = mmap(0, sz, PROT_READ| PROT_WRITE | PROT_EXEC,
                MAP_PRIVATE | MAP_LOCKED | MAP_ANONYMOUS, -1, 0);

        list_head *n = NULL;
        for(size_t i= 0; i< sz; i+=64)
        {
            n = (list_head *)((char *)p+i);
            memcpy((void *)&(((BindThisData*)((char *)p+i))->code),
                (void *)(&conet_bind_this_jump_help), 32);
            INIT_LIST_HEAD(n);
            list_add_tail(n, &m_free_list);
        }
    }

    BindThisData *get_obj()
    {
       if (list_empty(&m_free_list))
       {
            expand();
       }
       list_head *n =list_pop_head(&m_free_list);
       return (BindThisData *)(n);
    }

    void free_obj(BindThisData *d)
    {
        list_head  *n = (list_head *)(d);
        INIT_LIST_HEAD(n);
        list_add_tail(n, &m_free_list);
    }

    list_head m_free_list;
};


static __thread BindThisMgr * g_bind_this_mgr=NULL;

CONET_DEF_TLS_VAR_HELP_DEF(g_bind_this_mgr);

BindThisData * get_bind_this_data()
{
    return TLS_GET(g_bind_this_mgr)->get_obj();
}

void free_bind_this_data(BindThisData *d)
{
    return TLS_GET(g_bind_this_mgr)->free_obj(d);
}

}
