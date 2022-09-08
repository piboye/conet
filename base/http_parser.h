#ifndef HTTP_PARSER_H
#define HTTP_PARSER_H

#include <sys/types.h>
#include "ref_str.h"

namespace conet
{

typedef struct http_header_t
{
  ref_str_t name;
  ref_str_t value;
} http_header_t;

enum {
    CONNECTION_CLOSE=0,
    CONNECTION_KEEPALIVE=1,
};

enum {
    METHOD_GET=0,
    METHOD_POST=1,
};

enum {
    HTTP_REQUEST = 1,
    HTTP_RESPONSE = 1,
};

enum {
    HTTP_1_0 =1,
    HTTP_1_1 =0,
};

struct http_request_t
{
  char * body;
  char * mark;
  char * query_start;
  char * header_start;
  char * header_val_start;

  int status;

  struct{
  unsigned int err_too_many_header:1;
  unsigned int http_type:2; // 1 HTTP_REUQST, 2 HTTP_RESPONSE
  };

  size_t content_length;
  size_t nread;

  int version;
  int method;
  int connection;
  int status_code;

  struct {
    unsigned int websocket:1;
  };

  ref_str_t host;
  ref_str_t fragment;
  ref_str_t path;
  ref_str_t query_string;
  ref_str_t uri;
  ref_str_t upgrade;

  ref_str_t accept;
  ref_str_t content_type;
  ref_str_t reason_phrase;

  ref_str_t sec_websocket_key;
  ref_str_t sec_websocket_version;

  int headers_num;
  struct http_header_t headers[100];

  char *rest_data;  // 没有解析的数据
  size_t rest_len;

} __attribute__((aligned(64)));

void http_request_init(http_request_t *parser);

int http_request_finish(http_request_t *parser);

size_t http_request_parse(http_request_t *parser, char *data, size_t len, size_t off);

int http_request_has_error(http_request_t *parser);

int http_request_is_finished(http_request_t *parser);


inline size_t http_request_nread(http_request_t *parser)
{
    return parser->nread;
}

}

#endif
