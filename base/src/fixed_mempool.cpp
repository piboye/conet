/*
 * =====================================================================================
 *
 *       Filename:  mempool.cpp
 *
 *    Description:  
 *
 *        Version:  1.0
 *        Created:  2014年10月29日 21时29分56秒
 *       Revision:  none
 *       Compiler:  gcc
 *
 *         Author:  piboye
 *   Organization:  
 *
 * =====================================================================================
 */
#include <stdlib.h>
#include <unistd.h>
#include <sys/mman.h>
#include <unistd.h>
#include <malloc.h>
#include "fixed_mempool.h"

namespace conet
{

static int64_t g_page_size  = sysconf(_SC_PAGESIZE);

void * fixed_mempool_t::alloc_mem_help()
{
    if (is_page_alloc) {
        return mmap(NULL, alloc_size, PROT_READ| PROT_WRITE, MAP_PRIVATE | MAP_ANONYMOUS, -1, 0); 
    } else {
        if (align_size == 0)  {
            return malloc(alloc_size);
        } else {
            return  memalign(align_size, alloc_size); 
        }
    }
}

void fixed_mempool_t::free_mem_help(void *e)
{
    if (is_page_alloc) {
        munmap(e, alloc_size);
    } else {
        free((void *)e);
    }
}

int fixed_mempool_t::init(uint64_t alloc_size,  uint64_t max_num, int align_size)
{
    this->total_num = 0;
    this->used_num = 0;
    this->max_num = max_num;
    if (g_page_size ==0) {
        g_page_size = syscall(_SC_PAGESIZE); 
        if (g_page_size <=0) {
            g_page_size = -1;
        }
    }

    if (g_page_size > 0 && alloc_size >= (uint64_t)g_page_size) {
        alloc_size = (alloc_size + g_page_size -1) / g_page_size * g_page_size;
        this->is_page_alloc = 1;
    } else {
        this->is_page_alloc = 0;
        if (align_size > 0) {
            alloc_size = (alloc_size + align_size -1) / align_size * align_size;
        }
    }
    this->alloc_size = alloc_size;
    this->align_size = align_size;
    INIT_LIST_HEAD(&this->free_list);
    return 0;
}

void *fixed_mempool_t::alloc()
{
    list_head * n = list_pop_head(&free_list);
    if ( NULL == n) {
        n = (list_head *) alloc_mem_help();
        ++this->total_num;
    } 
    ++this->used_num;
    return n;
}

void fixed_mempool_t::free(void * obj)
{
    list_head *n = (list_head *)(obj);
    INIT_LIST_HEAD(n);
    if (this->total_num >= this->max_num)
    {
       --this->total_num;
       free_mem_help(n);
    } else {
        list_add(n, &this->free_list); 
    }
    --this->used_num;
}

void fixed_mempool_t::fini()
{
    list_head *e = NULL, *n =NULL;
    list_for_each_safe(e, n,  &this->free_list)
    {
        list_del(e);
        free_mem_help(e);
    }
}

}
