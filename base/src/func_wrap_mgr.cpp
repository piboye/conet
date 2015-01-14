/*
 * =====================================================================================
 *
 *       Filename:  func_wrap_mgr.cpp
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
#include <stdlib.h>
#include "list.h"
#include "func_wrap.h"
#include <sys/mman.h>
#include "tls.h"

namespace conet
{

struct FuncWrapMgr
{
    FuncWrapMgr()
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
            n = (list_head *)(p+i);
            memcpy(&(((FuncWrapData*)(p+i))->code),
                (void *)(&jump_to_real_func), 32);
            INIT_LIST_HEAD(n);
            list_add_tail(n, &m_free_list);
        }
    }

    FuncWrapData *get_obj()
    {
       if (list_empty(&m_free_list))
       {
            expand();
       }
       list_head *n =list_pop_head(&m_free_list);
       return (FuncWrapData *)(n);
    }

    void free_obj(FuncWrapData *d)
    {
        list_head  *n = (list_head *)(d);
        INIT_LIST_HEAD(n);
        list_add_tail(n, &m_free_list);
    }

    list_head m_free_list;
};


static FuncWrapMgr * g_func_wrap_mgr=NULL;
CONET_DEF_TLS_VAR_HELP_DEF(g_func_wrap_mgr);

FuncWrapData * get_func_wrap_data()
{
    return TLS_GET(g_func_wrap_mgr)->get_obj();
}

void free_func_wrap_data(FuncWrapData *d)
{
    return TLS_GET(g_func_wrap_mgr)->free_func_wrap_data(d);
}

}
