/*
 * =====================================================================================
 *
 *       Filename:  fifo.h
 *
 *    Description:  
 *
 *        Version:  1.0
 *        Created:  2014年10月27日 17时35分52秒
 *       Revision:  none
 *       Compiler:  gcc
 *
 *         Author:  piboye
 *   Organization:  
 *
 * =====================================================================================
 */
#ifndef __CONET_FIFO_H__
#define __CONET_FIFO_H__

#include "list.h"

namespace conet
{
    struct Fifo
    {

        struct Node
        {
            list_head link;
            void *value;
            Node()
            {
                INIT_LIST_HEAD(&link);
            }
        };
        typedef Node node_type;

        list_head used_list;
        list_head free_list;

        int (*obj_fini_func)(void *, void *);
        void * obj_fini_arg;

        Fifo()
        {
            INIT_LIST_HEAD(&used_list);
            INIT_LIST_HEAD(&free_list);

            obj_fini_func = NULL; 
            obj_fini_arg = NULL;
        }

        void push(void *v)
        {
            node_type *n = NULL;
            if (list_empty(&free_list)) {
                n = new node_type();
                n->value = v;
                list_add_tail(&n->link, &used_list);
            } else {
                n = container_of(free_list.next, node_type, link);
                n->value = v;
                list_move_tail(&n->link, &used_list); 
            }
            if (NULL == n) {
                n = new node_type();
            }
        }


        bool empty() {
            return list_empty(&used_list);
        }

        void * pop()
        {
            if (list_empty(&used_list)) 
            {
                return NULL;
            }
            node_type * n= container_of((used_list.next), node_type, link);
            list_move(&n->link, &free_list);
            return n->value;
        }

        void set_delete_obj_func(int (*fn)(void *arg, void * value), void *arg)
        {
            obj_fini_func = fn;
            obj_fini_arg = arg;
        }

        ~Fifo()
        {
            node_type *e = NULL;
            list_for_each_entry(e, &used_list, link)
            {
                if (obj_fini_func)
                {
                    obj_fini_func(obj_fini_arg, e->value);
                }
                delete e;
            }
            list_for_each_entry(e, &free_list, link)
            {
                delete e;
            }
        }
        
    };

}


#endif /* end of include guard */
