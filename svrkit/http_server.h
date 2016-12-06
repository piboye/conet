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

#include "tcp_server.h"
#include <map>
#include <string>
#include <vector>
#include "base/http_parser.h"

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
   void *extend;
   void (*free_fn)(void *);

   http_cmd_t()
   {
       proc = NULL;
       free_fn = NULL;
       arg = NULL;
       extend = NULL;
   }

   ~http_cmd_t()
   {
        if (free_fn)
        {
            free_fn(this->arg);
        }
   }
};

struct websocket_conn_t
{
    // close connection when set 1
    int to_close;
    conn_info_t * conn_info;
    http_request_t *http_first_req;
    http_server_t  *server;
    list_head link_to;
    llist_head m_send_queue;

    char * first_buffer;
    size_t first_len;

    websocket_conn_t()
    {
        to_close = 0;
        INIT_LIST_HEAD(&link_to);
        init_llist_head(&m_send_queue);
    }

    int do_handshake();
};


struct http_server_t: public server_base_t
{
    http_server_t();
    ~http_server_t();
    struct tcp_server_t *tcp_server;

    std::map<std::string, http_cmd_t*> cmd_maps;

    struct
    {
        unsigned int enable_keepalive:1;
        unsigned int enable_websocket:1;
    };

    void *extend;

    list_head m_conn_list;

    int registry_cmd(std::string const & name,  http_callback proc, void *arg,
        void (*free_arg)(void *) = NULL);

    http_cmd_t * get_http_cmd(std::string const &name);

    int init(tcp_server_t *tcp_server);
    int start();

    int do_stop(int wait);

    int conn_proc(conn_info_t *conn);
};

}


#endif /* end of include guard */
