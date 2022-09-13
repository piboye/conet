#pragma once

#include <stdlib.h>
#include <stdint.h>
#include <string_view>

struct redis_parser_t
{
    void reinit();

    // 参数个数
    int argnum=0;


    std::string_view *args=NULL;

    // 当前参数
    int cur_arg_pos=0;
    int cur_arg_size=0;
    char const * cur_arg_p=NULL;
    std::string_view *cur_arg=NULL;
    int cur_arg_char=0;

    enum {
        None=0,
        SET = 1,
        GET = 2,
    };

    int cmd = 0;

    int status = 0;
    int nread = 0;

    uint64_t _reserve[100];
};

int redis_parser_exec(redis_parser_t *sc, const char *data, int len, int off);
int redis_parser_finish(redis_parser_t *sc);