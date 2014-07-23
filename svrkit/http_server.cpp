/*
 * =====================================================================================
 *
 *       Filename:  http_server.cpp
 *
 *    Description:  
 *
 *        Version:  1.0
 *        Created:  2014年07月23日 17时23分11秒
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

class HttpServer
{
public:

    http_parser_settings settings;
    server_t server;

    int init(char const *ip, int port) 
    {
        this->server.proc = &this->server_proc;
        this->server.extend = this;
        
        init_sever(&this->server, ip, port);

        settings.on_url = my_url_callback;
        settings.on_header_field = my_header_field_callback;

    }

    int server_proc(conn_info_t *conn) 
    {

        http_parser *parser = malloc(sizeof(http_parser));
        http_parser_init(parser, HTTP_REQUEST);
        parser->data = conn->fd;

        size_t len = 80*1024;
        char buf[len];
        ssize_t nparsed = 0;
        do 
        {
            ssize_t recved;

            recved = recv(fd, buf, len, 0);

            if (recved < 0) {
                return 0;
            }
            nparsed = http_parser_execute(parser, &settings, buf, recved);
        } while(nparsed == );
        return 0;
    }
};

