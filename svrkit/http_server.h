/*
 * =====================================================================================
 *
 *       Filename:  http_server.h
 *
 *    Description:  
 *
 *        Version:  1.0
 *        Created:  2014年07月26日 23时41分58秒
 *       Revision:  none
 *       Compiler:  gcc
 *
 *         Author:  piboyeliu
 *   Organization:  
 *
 * =====================================================================================
 */
#ifndef HTTP_SERVER_H
#define HTTP_SERVER_H

#include "server_base.h"
#include "http_parser.h"
#include <map>
#include <string>
#include <vector>

namespace conet
{

struct http_server_t;

struct http_response_t
{
    int http_code;
    int keepalive;
    std::vector<std::string> headers;
    std::string body;
};


void response_to(http_response_t *resp, int http_code, std::string const &body);
void response_format(http_response_t *resp, int http_code, char const *fmt, ...);

void init_http_response(http_response_t *self);

int output_response(http_response_t *resp, int fd);

struct http_ctx_t
{
    int to_close; // close connection when set 1
    conn_info_t * conn_info;
    http_server_t *server;
    void * arg;
};

typedef int (*http_callback)(void *, http_ctx_t *ctx, http_request_t *req, http_response_t *resp);

struct http_cmd_t
{
   http_callback proc;
   void *arg; 
   std::string name;
};


struct http_server_t
{
    struct server_t *server;
    std::string server_name;
    std::map<std::string, http_cmd_t> cmd_maps;

    struct {
        unsigned int enable_keepalive:1;
    };
    void *extend;
};

int http_server_proc(conn_info_t *conn);
int http_server_proc2(conn_info_t *conn, 
        server_t *base_server, http_server_t *http_server);

http_cmd_t * get_http_cmd(http_server_t *server, std::string const &name);
int start_server(http_server_t *server);
int registry_cmd(http_server_t *server, std::string const & name,  http_callback proc, void *arg );
}


#endif /* end of include guard */
