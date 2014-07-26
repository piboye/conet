/*
 * =====================================================================================
 *
 *       Filename:  http_parser_test.cpp
 *
 *    Description:  
 *
 *        Version:  1.0
 *        Created:  2014年07月26日 15时01分22秒
 *       Revision:  none
 *       Compiler:  gcc
 *
 *         Author:  piboyeliu
 *   Organization:  
 *
 * =====================================================================================
 */
#include <stdlib.h>
#include "http_parser.h" 
#include "thirdparty/gtest/gtest.h"

using namespace conet;

TEST(http_request, parse)
{
    http_request_t req;
    http_request_init(&req);
    char buff[]=
        "POST / HTTP/1.1\r\n"
        "A: a\r\n"
        "B: b\r\n"
        "Content-Length: 5\r\n"
        "Content-Type: text-plain\r\n"
        "\r\n"
        "fooba";

    http_request_parse(&req, buff, sizeof(buff), 0);
    int status = http_request_finish(&req);
    printf("status:%d, %d\n", (int) req.status, (int)req.err_too_many_header);
    
    EXPECT_EQ(status, 1); 
    EXPECT_EQ(5, req.content_length);
    char hold = 0;
    EXPECT_STREQ("text-plain", ref_str_to_cstr(&req.content_type, &hold));
    EXPECT_STREQ("a", ref_str_to_cstr(&req.headers[0].value, &hold));
    EXPECT_STREQ("b", ref_str_to_cstr(&req.headers[1].value, &hold));
    EXPECT_STREQ("fooba", req.body);
}
