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
#include "base/pbc.h"
#include "base/ref_str.h"
#include <string.h>
#include "base/net_tool.h"
#include "google/protobuf/message.h"
#include "base/plog.h"

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

    // 可能的长度值
    uint32_t get_maybe_size()
    {
        return sizeof(cmd_base_t) - 3 *sizeof(uint32_t) + body.len + cmd_name.len + errmsg.len;
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

    inline
    int serialize_common(pb_buff_t &pb_buff)
    {
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

        if (ret >0) {
            pb_add_fixed32(&pb_buff, 6, (uint32_t)(ret));
        }

        if (errmsg.len >0) {
            pb_add_string(&pb_buff, 7, errmsg.data, errmsg.len);
        }

        return 0;
    }

    inline
    int serialize_reset(pb_buff_t &pb_buff)
    {
        if (body.len >0) {
            pb_add_string(&pb_buff, 5, body.data, body.len);
        }
        return 0;
    }
    


    int serialize_to(char const *buff, uint32_t len, uint32_t *out_len)
    {
        pb_buff_t pb_buff;
        pb_init_buff(&pb_buff, (void *)buff, (size_t)len);

        serialize_common(pb_buff);
        serialize_reset(pb_buff);

        *out_len = pb_get_encoded_length(&pb_buff);
        return 0;
    }
};

inline static
int send_cmd_base(int fd, PacketStream *ps,  cmd_base_t *cmd_base, google::protobuf::Message const *msg, int timeout)
{
    ps->init(fd);
    uint32_t out_len = 0;
    int ret = 0;

    pb_buff_t pb_buff;
    pb_init_buff(&pb_buff, (void *)(ps->buff+4), ps->max_size -4);
    
    cmd_base->serialize_common(pb_buff);
    if (msg) 
    {
        uint32_t rsp_len = msg->ByteSize();
        ret = pb_add_string_head(&pb_buff, 5, rsp_len);
        if (ret) {
            return -1;
        }

        if (pb_buff.left - rsp_len<=0) 
        {
            PLOG_ERROR("pb_buff.left=", pb_buff.left, ", rsp_len=", rsp_len);
            return -2;
        }

        msg->SerializeWithCachedSizesToArray((uint8_t *)pb_buff.ptr);

        pb_buff.ptr += rsp_len;
        pb_buff.left -= rsp_len;
    }

    out_len = pb_get_encoded_length(&pb_buff);

     
    *((uint32_t *)ps->buff) = htonl(out_len);

    ret = send_data(fd, ps->buff, out_len+4, timeout);

    return ret;
}

inline static
int serialize_cmd_base_impl(char*out, size_t *len,  cmd_base_t *cmd_base, google::protobuf::Message const *msg, uint32_t msg_len)
{
    uint32_t out_len = 0;
    int ret = 0;
    uint32_t max_len = *len;

    if (cmd_base->get_maybe_size() + msg_len < max_len) {
        return -1;
    }

    char *ptr = out;

    pb_buff_t pb_buff;

    pb_init_buff(&pb_buff, (void *)(ptr), max_len);
    
    cmd_base->serialize_common(pb_buff);
    if (msg) 
    {
        ret = pb_add_string_head(&pb_buff, 5, msg_len);
        if (ret) {
            return -1;
        }

        if (pb_buff.left - msg_len<=0) 
        {
            return -2;
        }

        msg->SerializeWithCachedSizesToArray((uint8_t *)pb_buff.ptr);

        pb_buff.ptr += msg_len;
        pb_buff.left -= msg_len;
    }

    out_len = pb_get_encoded_length(&pb_buff);

    *len = out_len;
    return 0;
}

static inline
int serialize_cmd_base(char*out, size_t *len,  cmd_base_t *cmd_base, google::protobuf::Message const *msg)
{
    if (*len <=4) {
        return -1;
    }
    uint32_t msg_len = 0;
    if (msg ) {
        msg_len = msg->ByteSize();
    }
    int ret = 0;
    *len-=4;
    ret = serialize_cmd_base_impl(out+4, len, cmd_base, msg, msg_len);
    if (ret) {
        return ret;
    }
     
    uint32_t out_len = *len;
    *((uint32_t *)(out)) = htonl(out_len);

    *len = out_len+4;
    return 0;
}

static inline
int serialize_cmd_base(std::vector<char> *out,  cmd_base_t *cmd_base, google::protobuf::Message const *msg)
{
    int ret = 0;
    uint32_t max_len = cmd_base->get_maybe_size();

    uint32_t msg_len = 0;
    if (msg ) {
        msg_len = msg->ByteSize();
        max_len += msg_len;
    }

    out->resize(max_len);

    size_t len = out->size()-4;

    ret = serialize_cmd_base_impl(&(*out)[4], &len, cmd_base, msg, msg_len);
    if (ret) {
        return ret;
    }
     
    uint32_t out_len = len;
    *((uint32_t *)(&(*out)[0])) = htonl(out_len);

    out->resize(out_len+4);
    return 0;
}

}

#endif /* end of include guard */
