/*
 * =====================================================================================
 *
 *       Filename:  rpc_base_pb_test.cpp
 *
 *    Description
 *
 *        Version:  1.0
 *        Created:  2014年11月09日 07时19分11秒
 *       Revision:  none
 *       Compiler:  gcc
 *
 *         Author:  YOUR NAME (), 
 *   Organization:  
 *
 * =====================================================================================
 */
#include <stdlib.h>

#include "svrkit/rpc_base_pb.pb.h"
#include "../../base/incl/pbc.h"
#include "gtest/gtest.h"


TEST(pbc, decode)
{
    conet_rpc_pb::CmdBase msg;
    msg.set_type(1);
    msg.set_cmd_name("abc");
    msg.set_cmd_id(1);
    msg.set_seq_id(2);
    msg.set_body("abcdef");

    std::string txt;
    msg.SerializeToString(&txt);

    pb_field_t start1;

    pb_field_t *f = pb_begin(&start1, txt.c_str(), txt.size());
    ASSERT_EQ(1, (int)f->val.i32);

    f = pb_next(f);

    ASSERT_STREQ("abc", std::string(f->val.str.data, f->val.str.len).c_str());

    f = pb_next(f);
    ASSERT_EQ(1, (int)f->val.i64);

    f = pb_next(f);
    ASSERT_EQ(2, (int)f->val.i64);

    f = pb_next(f);
    ASSERT_STREQ("abcdef", std::string(f->val.str.data, f->val.str.len).c_str());

    f = pb_next(f);

    ASSERT_EQ(NULL, f);
}
