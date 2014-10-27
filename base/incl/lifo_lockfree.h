/*
 * =====================================================================================
 *
 *       Filename:  lifo_lockfree.h
 *
 *    Description:  
 *
 *        Version:  1.0
 *        Created:  2014年10月27日 11时48分38秒
 *       Revision:  none
 *       Compiler:  gcc
 *
 *         Author:  piboye
 *   Organization:  
 *
 * =====================================================================================
 */

#ifndef __LIFO_LOCK_FREE_H__
#define __LIFO_LOCK_FREE_H__

#include <stddef.h>
#include <stdint.h>
#include <stdlib.h>
#include <malloc.h>

namespace conet 
{

class lifo_lockfree_t 
{
public:

    struct node_t;

    struct pointer_t 
    {
        node_t *ptr;
        uint64_t tag; // for ABA problem in cas2
        pointer_t() 
        {
            ptr = NULL;
            tag = 0;
        }

        pointer_t(node_t *a_ptr,  uint64_t a_tag) 
        {
            ptr = a_ptr; 
            tag=a_tag;
        }

        pointer_t(pointer_t const & a)
        {
            ptr = a.ptr;
            tag = a.tag;
        }

    }
    __attribute__ ((packed, aligned (16)))
    ;

    struct node_t 
    { 
        volatile pointer_t next; 
        void * value; 
        node_t() 
        {
            value = NULL; // dummy_val
            next.ptr = NULL; 
            next.tag = 0;
        }

        void reinit(void *val = NULL)
        {
            next.ptr = NULL; 
            next.tag = 0;
            value = val;
        }
    };


    static 
    inline
    bool CAS2(pointer_t volatile *addr,
                pointer_t &old_value,
                pointer_t &new_value)
    {
            bool  ret;
            __asm__ __volatile__(
                    "lock cmpxchg16b %1;\n"
                    "sete %0;\n"
                    :"=m"(ret),"+m" (*(volatile pointer_t *) (addr))
                    :"a" (old_value.ptr), "d" (old_value.tag), "b" (new_value.ptr), "c" (new_value.tag));

            return ret;
    }

public:
    //var members
    volatile pointer_t head_;


public:

    lifo_lockfree_t() 
    {

    }

    static
    node_t * alloc_node()
    {

        node_t *nd = (node_t *)memalign(16, sizeof(node_t)); 
        nd->reinit(NULL);
        return nd;
    }

    static
    void free_node(node_t *nd) 
    {
        free(nd);
    }

    void init()
    {
        node_t *nd = alloc_node();
        head_.ptr = nd;
        head_.tag = 0;
    }

    void push(node_t *nd, void * val) 
    {
        pointer_t head, next;
        nd->value = val;
        nd->next.ptr = NULL;

        while(1)
        {
            head.ptr = this->head_.ptr;
            head.tag = this->head_.tag;
            nd->next.ptr = head.ptr;
            nd->next.tag = head.tag+1;
            if ((head.ptr == this->head_.ptr) &&
                 (head.tag == this->head_.tag))
            {
                pointer_t new_pt;
                new_pt.ptr = (node_t *)nd;
                new_pt.tag = head.tag+1;
                if (CAS2(&this->head_, head, new_pt)) {
                    break;
                }
            }
        }
    } 

    bool empty() const
    {
        return head_.ptr == NULL;
    }


    node_t * pop() 
    {

        pointer_t head;
        node_t * nd = NULL;
        while(1)
        { 
            head.ptr = this->head_.ptr; 
            head.tag = this->head_.tag; 

            if(head.ptr == NULL) {
                return NULL;
            }

            nd = head.ptr;

            if ( 
                (head.ptr == this->head_.ptr)  &&
                (head.tag == this->head_.tag) 
               )
            {
                pointer_t new_pt(head.ptr->next.ptr, head.tag+1);
                if (CAS2(&(this->head_), head, new_pt)) {
                    break;
                }
            }
        }

        if (nd) {
            nd->next.ptr = NULL;
        }

        return nd;
    }
};

}

#endif 
