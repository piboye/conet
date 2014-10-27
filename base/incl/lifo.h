/*
 * =====================================================================================
 *
 *       Filename:  lifo.h
 *
 *    Description:  
 *
 *        Version:  1.0
 *        Created:  2014年10月27日 14时15分46秒
 *       Revision:  none
 *       Compiler:  gcc
 *
 *         Author:  piboye
 *   Organization:  
 *
 * =====================================================================================
 */
#ifndef __CONET_LIFO_H__
#define __CONET_LIFO_H__

#include "list.h"
namespace conet
{

    struct lifo_t
    {
        struct node_t
        {
            node_t *next;
            void *value;
        };

        node_t *head;

        lifo_t() 
        {
            head = NULL;
        }

        void push(node_t *n)
        {
            n->next = head;
            head = n;
        }

        node_t *pop()
        {
           node_t * n = head; 
           if (n) 
           {
               head = n->next;
           }
           return n;
        }

        bool empty() const 
        {
            return head == NULL;
        }
    };

    
    struct Lifo
    {
        lifo_t used_list;
        lifo_t free_list;

        typedef lifo_t::node_t node_type;

        void push(void *v)
        {
            node_type *n = free_list.pop();
            if (NULL == n) {
                n = new node_type();
            }
            n->value = v;
            used_list.push(n);
        }

        bool empty() {
            return used_list.empty();
        }

        void * pop()
        {
            node_type * n= used_list.pop();
            if (NULL == n) {
                return NULL;
            }
            free_list.push(n);
            return n->value;
        }

        ~Lifo()
        {
            node_type *n = NULL;
            while( (n = used_list.pop()))
            {
                delete n;
            }
            while( (n = free_list.pop()))
            {
                delete n;
            }
        }
    };

}

#endif /* end of include guard */
