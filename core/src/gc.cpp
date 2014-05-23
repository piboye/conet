/*
 * =====================================================================================
 *
 *       Filename:  gc.cpp
 *
 *    Description:
 *
 *        Version:  1.0
 *        Created:  2014年05月14日 08时10分32秒
 *       Revision:  none
 *       Compiler:  gcc
 *
 *         Author:  piboyeliu
 *   Organization:
 *
 * =====================================================================================
 */
#include <assert.h>
#include <stdlib.h>
#include "gc.h"

namespace conet
{

void gc_free_all(gc_mgr_t *mgr)
{
    list_head * it = NULL, *next = NULL;
    list_for_each_safe(it, next, &mgr->alloc_list)
    {
        gc_block_t * block = container_of(it, gc_block_t, link);
        if (block->destructor) {
            block->destructor(block->data, block->num);
        }
        list_del(&block->link);
        free(block);
    }
}

void init_gc_mgr(gc_mgr_t *mgr)
{
    INIT_LIST_HEAD(&mgr->alloc_list);
}

void gc_free(void *p)
{
    assert(p);
    gc_block_t *block = container_of(p, gc_block_t, data);
    if (block->destructor) {
        block->destructor(block->data, block->num);
    }
    list_del(&block->link);
    free(block);
}

}

