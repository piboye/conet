/*
 * =====================================================================================
 *
 *       Filename:  ref_str.h
 *
 *    Description:  
 *
 *        Version:  1.0
 *        Created:  2014年07月26日 18时27分12秒
 *       Revision:  none
 *       Compiler:  gcc
 *
 *         Author:  piboyeliu
 *   Organization:  
 *
 * =====================================================================================
 */

#ifndef REF_STR_H
#define REF_STR_H

namespace conet
{

typedef struct ref_str_t
{
   size_t len;
   char * data;
} ref_str_t;


inline
void init_ref_str(struct ref_str_t * s, char *start, size_t len)
{
    s->len = len;
    s->data = start; 
}

inline
void init_ref_str(struct ref_str_t * s, char *start, char * end)
{
    s->len = end-start;
    s->data = start; 
}

inline
char * ref_str_to_cstr(struct ref_str_t *s, char *hold)
{
    *hold = s->data[s->len];
    s->data[s->len]=0;
    return s->data;
}

inline 
void ref_str_restore(struct ref_str_t *s, char hold)
{
    s->data[s->len] = hold;
}

}
#endif /* end of include guard */
