#ifndef HTTP_PARSER_H
#define HTTP_PARSER_H

#include <sys/types.h>
#include "core/incl/ref_str.h"

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
    HTTP_1_0 =1,
    HTTP_1_1 =0,
};

typedef 
struct http_request_t 
{ 

  char *body;
  char *mark;
  char *query_start;
  char * header_start;
  char * header_val_start;



  int status;

  struct{
  unsigned int err_too_many_header:1;
  };

  size_t content_length;
  size_t nread;

  int version;
  int method;
  int connection;

  
  ref_str_t host;
  ref_str_t fragment;
  ref_str_t path;
  ref_str_t query_string;
  ref_str_t uri;
  

  ref_str_t accept;
  ref_str_t content_type;

  int headers_num;
  struct http_header_t headers[100];

} http_request_t; 

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
