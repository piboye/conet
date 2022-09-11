/*
 * =====================================================================================
 *
 *       Filename:  ref_str_tool.h
 *
 *    Description:  
 *
 *        Version:  1.0
 *        Created:  2014年10月29日 05时37分10秒
 *       Revision:  none
 *       Compiler:  gcc
 *
 *         Author:  YOUR NAME (), 
 *   Organization:  
 *
 * =====================================================================================
 */
#include "ref_str.h"
namespace conet
{
    inline 
    bool ref_str_empty(ref_str_t str) 
    {
        return str.len == 0;
    }
 
}
