/*
 * =====================================================================================
 *
 *       Filename:  fifo_lockfree.h
 *
 *    Description:  
 *
 *        Version:  1.0
 *        Created:  2014年10月27日 05时48分38秒
 *       Revision:  none
 *       Compiler:  gcc
 *
 *         Author:  piboye
 *   Organization:  
 *
 * =====================================================================================
 */

#ifndef __FIFO_LOCK_FREE_H__
#define __FIFO_LOCK_FREE_H__

#include <stddef.h>
#include <stdint.h>
#include <stdlib.h>
#include <malloc.h>
#include <assert.h>

namespace conet 
{

class fifo_lockfree_t 
{
public:
    typedef void * data_type;

    struct node_t;

    struct pointer_t 
    {
        node_t *ptr;
        uint64_t tag;
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
        pointer_t next; 
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
    } __attribute__ ((aligned (16)));


    pointer_t tail_;
    pointer_t head_;

    struct node_t init_node_;

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

    fifo_lockfree_t() 
    {
    }

    void init() volatile
    {
        node_t *nd = (node_t *)memalign(16, sizeof(node_t)); 
        nd->reinit(NULL);
        head_.ptr = nd;
        head_.tag = 0;
        tail_.ptr = nd;
        tail_.tag = 0;
    }

    void push(node_t *nd, void * val) volatile
    {
        pointer_t tail, next;
        nd->value = val;
        nd->next.ptr = NULL;

        while(1)
        {
            tail.ptr = this->tail_.ptr;
            tail.tag = this->tail_.tag;
            next.ptr = tail.ptr->next.ptr;
            next.tag = tail.ptr->next.tag;
            if ((tail.ptr == this->tail_.ptr) &&
                 (tail.tag == this->tail_.tag))
            {
                if(next.ptr == NULL) {
                    pointer_t new_pt;
                    new_pt.ptr = (node_t *)nd;
                    new_pt.tag = next.tag+1;
                    nd->next.tag = new_pt.tag;
                    if(CAS2(&(this->tail_.ptr->next), next, new_pt)){ 
                        break; // Enqueue done!
                    }
                }else {
                    pointer_t new_pt(next.ptr, tail.tag+1);
                    nd->next.tag = new_pt.tag;
                    CAS2(&(this->tail_), tail, new_pt); 
                }
            }
        }
        pointer_t new_pt(nd, tail.tag+1);
        CAS2(&(this->tail_), tail, new_pt);
    } 


    node_t * pop() volatile
    {

        pointer_t tail, head, next;
        void * value = NULL;
        while(1)
        { 
            head.ptr = this->head_.ptr; 
            head.tag = this->head_.tag; 
            tail.ptr = this->tail_.ptr; 
            tail.tag = this->tail_.tag; 
            next.ptr = (head.ptr)->next.ptr; 
            next.tag = (head.ptr)->next.tag; 
            if ( 
                (head.ptr == this->head_.ptr)  &&
                (head.tag == this->head_.tag) 
               )
            {

                if(head.ptr == tail.ptr){
                    if (next.ptr == NULL){ 
                        return NULL;
                    }
                    pointer_t new_pt(next.ptr, tail.tag+1);
                    CAS2(&(this->tail_), tail, new_pt);
                } else{ 
                    value = next.ptr->value;
                    pointer_t new_pt(next.ptr, head.tag+1);
                    if(CAS2(&(this->head_), head, new_pt)){
                        break;
                    }
                }
            }
        }
        node_t *nd = head.ptr;
        nd->value = value;
        return nd;
    }
};

}

#endif 
