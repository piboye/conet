/*
 * =====================================================================================
 *
 *       Filename:  cmd_base.h
 *
 *    Description:  
 *
 *        Version:  1.0
 *        Created:  11/08/2014 01:07:40 PM
 *       Revision:  none
 *       Compiler:  gcc
 *
 *         Author:  piboye
 *   Organization:  
 *
 * =====================================================================================
 */

#ifndef __CONET_CMD_BASE_H__
#define __CONET_CMD_BASE_H__
#include "../../base/incl/pbc.h"
#include "../../base/incl/ref_str.h"
#include <string.h>

namespace conet
{

struct cmd_base_t
{
    uint32_t type;  // 1
    ref_str_t cmd_name; //2
    uint64_t cmd_id;  //3 
    uint64_t seq_id; //4 
    ref_str_t body; // 5
    uint32_t ret;  // 6
    ref_str_t errmsg; //7

    void init()
    {
       memset(this, 0, sizeof(*this));
    }

    int parse(char const * buff, size_t len)
    {
        pb_field_t start;
        pb_field_t *f = pb_begin(&start, buff, len);

        while (f)
        {
            switch(f->tag)
            {
                case 1:
                    type = f->val.i32;
                    break;

                case 2:
                    init_ref_str(&cmd_name, f->val.str.data, f->val.str.len);
                    break;

                case 3:
                    cmd_id = f->val.i64;
                    break;

                case 4:
                    seq_id = f->val.i64;
                    break;

                case 5:
                    init_ref_str(&body, f->val.str.data, f->val.str.len);
                    break;

                case 6:
                    ret = f->val.i32;
                    break;

                case 7:
                    init_ref_str(&errmsg, f->val.str.data, f->val.str.len);
                    break;
            }
            f = pb_next(f);
        }
        return 0;
    }

    int serialize_to(char const *buff, uint32_t len, uint32_t *out_len)
    {
        pb_buff_t pb_buff;
        pb_init_buff(&pb_buff, (void *)buff, (size_t)len);

        if (type >0) { 
            pb_add_varint(&pb_buff, 1, (uint32_t)(type));  
        }


        if (cmd_name.len >0 && cmd_name.data) {
            pb_add_string(&pb_buff, 2, cmd_name.data, cmd_name.len);
        }

        if (cmd_id >0) {
            pb_add_varint(&pb_buff, 3, (uint64_t)(cmd_id));  
        }

        if (seq_id >0) {
            pb_add_varint(&pb_buff, 4, (uint64_t)(seq_id));  
        }

        if (body.len >0) {
            pb_add_string(&pb_buff, 5, body.data, body.len);
        }

        if (ret >0) {
            pb_add_fixed32(&pb_buff, 6, (uint32_t)(ret));
        }

        if (errmsg.len >0) {
            pb_add_string(&pb_buff, 7, errmsg.data, errmsg.len);
        }

        *out_len = pb_get_encoded_length(&pb_buff);
        return 0;
    }
};

}


#endif /* end of include guard */
