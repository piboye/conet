/*
 * =====================================================================================
 *
 *       Filename:  ptr_cast.h
 *
 *    Description:  
 *
 *        Version:  1.0
 *        Created:  2014年11月02日 22时18分47秒
 *       Revision:  none
 *       Compiler:  gcc
 *
 *         Author:  piboye
 *   Organization:  
 *
 * =====================================================================================
 */

#ifndef __PTR_CAST_H__
#define __PTR_CAST_H__

#include <string.h>

namespace conet 
{
template<typename dst_fn_t, typename src_fn_t>    
dst_fn_t ptr_cast(src_fn_t fn)
{
    dst_fn_t ret_fn=NULL;
    memcpy(&(ret_fn), &(fn), sizeof(void *));
    return ret_fn;
}

} // namespace conet

#endif /* end of include guard */
