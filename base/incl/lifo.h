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

        int (*obj_fini_func)(void *, void *);
        void * obj_fini_arg;


        typedef lifo_t::node_t node_type;

        Lifo()
        {
           obj_fini_func = NULL; 
           obj_fini_arg = NULL;
        }

        void push(void *v)
        {
            node_type *n = free_list.pop();
            if (NULL == n) {
                n = new node_type();
            }
            n->value = v;
            used_list.push(n);
        }


        bool empty() const
        {
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

        void set_delete_obj_func(int (*fn)(void *arg, void * value), void *arg)
        {
            obj_fini_func = fn;
            obj_fini_arg = arg;
        }

        ~Lifo()
        {
            node_type *n = NULL;
            while( (n = used_list.pop()))
            {
                if (obj_fini_func)
                {
                    obj_fini_func(obj_fini_arg, n->value);
                }
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
