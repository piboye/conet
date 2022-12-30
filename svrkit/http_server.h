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
#include "base/llist.h"
#include "base/list.h"
#include <list>

namespace conet
{

struct http_server_t;

struct http_response_t
{
    int http_code;
    int keepalive;
    std::vector<std::string> headers;
    std::string body;
    char const *data;
    size_t data_len;
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

struct ws_packet_head_t
{
    unsigned char opcode : 4;
    unsigned char RSV : 3;
    unsigned char FIN : 1;
    unsigned char payload_len:7;
    unsigned char MASK:1;
} __attribute__((packed));

struct ws_packet_t
{
    ws_packet_head_t head;
    uint64_t payload_len;
    uint32_t masking_key;
    char * data;
    uint64_t len;
    char * payload_data;

    ws_packet_t()
    {
        head.opcode = 0;
        head.RSV = 0;
        head.FIN = 0;
        head.payload_len = 0;
        head.MASK = 0;
        payload_len = 0;
        masking_key = 0;
        data = NULL;
        len = 0;
        payload_data = NULL;
    }
};


struct websocket_conn_t;

struct websocket_cb_t
{
    void *arg;
    websocket_conn_t *ws_conn;
    int (*do_new)(void *arg, websocket_conn_t *conn);
    int (*do_msg)(void *arg, websocket_conn_t *conn, ws_packet_t *pkg);
    int (*do_close)(void *arg, websocket_conn_t *conn);

    websocket_cb_t()
    {
        memset(this, 0, sizeof(*this));
    }
};

struct websocket_conn_t
{
    int to_close; // close connection when set 1
    int fd;
    conn_info_t * conn_info;
    http_request_t *http_first_req;
    http_server_t *server;
    list_head link_to;
    std::list<std::string *> send_queue;


    int32_t send_pos;

    char * first_buffer;
    size_t first_len;

    char * buffer;
    int64_t max_buf_len;
    int64_t data_pos;
    int64_t prev_data_pos;
    int32_t max_idle_time; // 秒为单位

    ws_packet_t req_package;

    enum
    {
        STOP = -1,
        READ_PAYLOAD_LEN=0,
        PARSE_PAYLOAD_LEN=1,
        PARSE_MASKING = 2,
        CALC_REST_SIZE = 3,
        READ_DATA = 4,
        PROC_MSG = 5,
    };

    int state;

    int ref_cnt;
    uint32_t server_masking_key;

    websocket_cb_t cb;

    void *extend;

    void add_ref()
    {
        ++this->ref_cnt;
    }

    void decr_ref()
    {
        --this->ref_cnt;
    }

    websocket_conn_t();
    ~websocket_conn_t();

//private:
    int do_handshake();
    int do_main();
    int do_read();
    int do_write();
    int do_proc_pkg(ws_packet_t *pkg);
    int do_rsp(char *data, uint64_t len, bool is_text = false);
    int do_rsp_with_mask(char *data, uint64_t len, uint32_t mask, bool is_text=false);
    int do_rsp(ws_packet_t * pkg);
    int do_rsp_close(char const *data=NULL, int32_t len = -1);

    // ping 包的响应处理
    int do_pong(ws_packet_t *pkg);
};



struct http_server_t: public server_base_t
{
    http_server_t();
    ~http_server_t();
    tcp_server_t *tcp_server;

    std::map<std::string, http_cmd_t*> cmd_maps;

    http_cmd_t * default_cmd;


    struct {
        unsigned int enable_keepalive:1;
        unsigned int enable_websocket:1;
        unsigned int enable_client_close:1;  // 客户端主动关闭
    };

    uint32_t websocket_masking_key;
    uint32_t websocket_max_idle_time;  // 单位是秒
    uint32_t websocket_max_packet_size;    // websocket packet 最大缓存大小

    void *extend;
    list_head conn_list;

    int registry_default_cmd(http_callback proc, void *arg,
            void (*free_arg)(void *) = NULL);

    int registry_cmd(std::string const & name,  http_callback proc, void *arg,
        void (*free_arg)(void *) = NULL);

    http_cmd_t * get_http_cmd(std::string const &name);

    // websocket 消息包处理函数
    websocket_cb_t ws_cb;

    int set_ws_proc(websocket_cb_t const &cb);

    int set_ws_mask_key(uint32_t mask);

    int init(tcp_server_t *tcp_server);
    int start();
    int do_stop();

    bool has_stoped();

    int conn_proc(conn_info_t *conn);
    int ws_main(conn_info_t *conn, http_request_t *req,
        tcp_server_t *server_base,
        http_server_t *http_server,
        char *buf,
        int len
        );
};

}

#endif /* end of include guard */
