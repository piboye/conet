/*
 * =====================================================================================
 *
 *       Filename:  string_tpl.h
 *
 *    Description:  
 *
 *        Version:  1.0
 *        Created:  07/06/2015 02:38:15 AM
 *       Revision:  none
 *       Compiler:  gcc
 *
 *         Author:  piboye
 *   Organization:  
 *
 * =====================================================================================
 */
#ifndef __CONET_STRING_TPL_H__
#define __CONET_STRING_TPL_H__

#include <string>
#include <map>

namespace conet
{

static 
inline 
int string_tpl(std::string const &tpl, 
        std::map<std::string, std::string> const & datas, 
        std::string *out, std::string *errmsg = NULL)
{
    static char const * pre_tag="{{";
    static char const * post_tag="}}";
    size_t pre_tag_len = 2;
    size_t post_tag_len = 2;
    size_t cur_pos = 0;
    size_t pre_pos = 0;
    size_t end_pos = 0;

    for (;;)
    {
        cur_pos = tpl.find(pre_tag, pre_pos, pre_tag_len);
        if (cur_pos != std::string::npos)
        {
            out->append(tpl, pre_pos, cur_pos-pre_pos);
            end_pos = tpl.find(post_tag, cur_pos+pre_tag_len, post_tag_len);
            if (end_pos != std::string::npos)
            {
                std::string name(tpl, cur_pos+pre_tag_len, end_pos - cur_pos - (pre_tag_len));
                typeof(datas.begin()) it = datas.find(name);
                if (it == datas.end()) {
                    // no found name in data , error
                    if (errmsg) {
                        *errmsg = "no found key data, key:";
                        *errmsg += name;
                    }
                    return -1;
                }
                // replace data
                out->append(it->second);
                pre_pos = end_pos + post_tag_len;
            } else {
                // {{ not end with }}
                return -2;
            }
        } else {
            // complete all data
            out->append(tpl, pre_pos, cur_pos);
            break;
        }
    }

    return 0;
}

}

#endif /* end of include guard */

