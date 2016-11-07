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
#include <string>
#include <string.h>
#include <stdint.h>

namespace conet
{

typedef struct ref_str_t
{
   uint32_t len;
   char * data;

} ref_str_t;

inline
ref_str_t ref_str(char const * src, size_t len) 
{
    ref_str_t d; 
    d.data = (char *)(src);
    d.len = (uint32_t) len;
    return d;
}

inline
ref_str_t ref_str(std::string const &v)
{
    ref_str_t d; 
    d.data = (char *)v.data();
    d.len = v.size();
    return d;
}

inline
void init_ref_str(ref_str_t *d, std::string const &v)
{
    d->data = (char *)v.data();
    d->len = v.size();
}

inline
void init_ref_str(struct ref_str_t * s, char const *start)
{
    s->len = (uint32_t)strlen(start);
    s->data = (char *)start; 
}

inline
ref_str_t ref_str(char const * data)
{
    ref_str_t d; 
    init_ref_str(&d, data);
    return d;
}

inline
void init_ref_str(struct ref_str_t * s, char const *start, size_t len)
{
    s->len = (uint32_t)len;
    s->data = (char *)start; 
}

inline
void init_ref_str(struct ref_str_t * s, char const *start, char const * end)
{
    s->len = (uint32_t)(end-start);
    s->data = (char *)start; 
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

inline 
void ref_str_to(struct ref_str_t *src, std::string *out)
{
    out->assign(src->data, src->len);
}

inline 
std::string ref_str_as_string(struct ref_str_t *src)
{
    std::string out;
    out.assign(src->data, src->len);
    return out;
}

inline 
bool equal(ref_str_t const &l, ref_str_t const & r)
{
    return (l.len == r.len) && (0 == (memcmp(l.data, r.data, l.len)));
}


}
#endif /* end of include guard */
