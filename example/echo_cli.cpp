/*
 * =====================================================================================
 *
 *       Filename:  echo_cli.cpp
 *
 *    Description:
 *
 *        Version:  1.0
 *        Created:  2014年05月11日 08时16分30秒
 *       Revision:  none
 *       Compiler:  gcc
 *
 *         Author:  YOUR NAME (),
 *   Organization:
 *
 * =====================================================================================
 */
#include <stdlib.h>
#include <stdlib.h>
#include <stdio.h>
#include "base/net_tool.h"


int main(int argc, char const* argv[])
{
    if (argc < 3) {
        fprintf(stderr, "usage:%s ip port\n", argv[0]);
        return 0;
    }
    char const * ip = argv[1];
    int  port = atoi(argv[2]);

    int ret = 0;
    int fd = 0;
    fd = conet::connect_to(ip, port);
    conet::set_none_block(fd, false);
    char *line= NULL;
    size_t len = 0;
    char rbuff[1024];
    while( (ret = getline(&line, &len, stdin)) >= 0) {
        if (ret == 0) continue;
        ret = write(fd, line, ret);
        if (ret <= 0) break;
        ret = read(fd, rbuff, 1024);
        if (ret <=0) break;
        write(1, rbuff, ret);
    }
    close(fd);
    free(line);
    return 0;
}

