/*
 * =====================================================================================
 *
 *       Filename:  rpc_base_test.cpp
 *
 *    Description:  
 *
 *        Version:  1.0
 *        Created:  2014年10月27日 09时56分30秒
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
#include "gtest/gtest.h"

TEST(p, a)
{
    conet_rpc_pb::CmdBase a, b;
    a.set_seq_id(123);
    std::string s;
    b.SerializeToString(&s);
    a.ParseFromString(s);
    ASSERT_EQ(0, (int)a.seq_id());
}

