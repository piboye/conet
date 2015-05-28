/*
 * =====================================================================================
 *
 *       Filename:  url_encode_test.cpp
 *
 *    Description:  
 *
 *        Version:  1.0
 *        Created:  05/28/2015 04:44:56 AM
 *       Revision:  none
 *       Compiler:  gcc
 *
 *         Author:  piboye
 *   Organization:  
 *
 * =====================================================================================
 */
#include <stdlib.h>

#include "thirdparty/gtest/gtest.h"
#include "url_encode.h"

using namespace conet;

TEST(url_decode, decode)
{
    std::string out;
    url_decode(std::string("%20abc"), &out);
    //ASSERT_EQ(3, (int)out.size());
    ASSERT_STREQ(" abc", out.c_str());
}

