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

#include "../incl/cmd_base.h"

using namespace conet;

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

    cmd_base_t cmd_base;
    cmd_base.init();
    cmd_base.parse(txt.c_str(), (uint32_t) txt.size());

    ASSERT_EQ(1, (int)cmd_base.type);

    ASSERT_STREQ("abc", ref_str_as_string(&cmd_base.cmd_name).c_str());

    ASSERT_EQ(1, (int)cmd_base.cmd_id);

    ASSERT_EQ(2, (int)cmd_base.seq_id);

    ASSERT_STREQ("abcdef", ref_str_as_string(&cmd_base.body).c_str());

}

TEST(pbc, encode)
{

    char buff[1024]={0};

    cmd_base_t cmd_base;
    cmd_base.init();
    cmd_base.type = 1;
    init_ref_str(&cmd_base.cmd_name, "abc", strlen("abc"));
    cmd_base.cmd_id = 1;
    cmd_base.seq_id = 2;
    init_ref_str(&cmd_base.body, "abcdef", strlen("abcdef"));

    uint32_t out_len = 0;
    cmd_base.serialize_to(buff, (uint32_t )sizeof(buff), &out_len);

    conet_rpc_pb::CmdBase msg;
    msg.ParseFromArray(buff, out_len);

    ASSERT_EQ(1, (int)msg.type());

    ASSERT_STREQ("abc", msg.cmd_name().c_str());

    ASSERT_EQ(1, (int) msg.cmd_id());

    ASSERT_EQ(2, (int) msg.seq_id());

    ASSERT_STREQ("abcdef", msg.body().c_str());

}
