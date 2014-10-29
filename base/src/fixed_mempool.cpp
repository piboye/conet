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
#include "fixed_mempool.h"

namespace conet
{

static int64_t g_page_size  = sysconf(_SC_PAGESIZE);
static void * alloc_mem_help(uint64_t size, int align = 0)
{
    if (size >= (uint64_t)g_page_size) {
        return mmap(NULL, size, PROT_READ| PROT_WRITE, MAP_PRIVATE | MAP_ANONYMOUS, -1, 0); 
    } else {
        return malloc(size);
    }
}

int fixed_mempool_t::init(uint64_t alloc_size,  uint64_t max_num, int align_size)
{
    this->total_num = 0;
    this->used_num = 0;
    this->max_num = max_num;
    this->alloc_size = alloc_size;
    this->align_size = align_size;
    INIT_LIST_HEAD(&this->free_list);
    return 0;
}

void *fixed_mempool_t::alloc()
{
    list_head * n = list_pop_head(&free_list);
    if ( NULL == n) {
        n = (list_head *) alloc_mem_help(this->alloc_size, this->align_size);
    } 
    return n;
}

void fixed_mempool_t::free(void * obj)
{
    list_head *n = (list_head *)(obj);
    list_del_init(n);
    list_add(n, &this->free_list); 
}

}
