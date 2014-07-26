/**
 * Copyright (c) 2005 Zed A. Shaw
 * You can redistribute it and/or modify it under the same terms as Ruby.
 */

#ifndef HTTP_PARSER_H
#define HTTP_PARSER_H

#include <sys/types.h>

namespace conet
{

typedef struct ref_str_t
{
   size_t len;
   char * data;
} ref_str_t;


inline
void init_ref_str(struct ref_str_t * s, char *start, size_t len)
{
    s->len = len;
    s->data = start; 
}

inline
void init_ref_str(struct ref_str_t * s, char *start, char * end)
{
    s->len = end-start;
    s->data = start; 
}

inline
char * ref_str_to_cstr(struct ref_str_t *s, char *hold)
{
    *hold = s->data[s->len];
    s->data[s->len]=0;
    return s->data;
}

inline 
void ref_str_restore(struct ref_str_t *s, char hold)
{
    s->data[s->len] = hold;
}

typedef struct http_header_t
{
  ref_str_t name;
  ref_str_t value;
} http_header_t;

typedef 
struct http_parser_t 
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
  
  ref_str_t fragment;
  ref_str_t version;
  ref_str_t path;
  ref_str_t method;
  ref_str_t query_string;
  ref_str_t uri;

  ref_str_t connection;
  ref_str_t accept;
  ref_str_t content_type;

  int headers_num;
  struct http_header_t headers[100];

} http_parser_t; 

void http_parser_init(http_parser_t *parser);

int http_parser_finish(http_parser_t *parser);

size_t http_parser_execute(http_parser_t *parser, char *data, size_t len, size_t off);

int http_parser_has_error(http_parser_t *parser);

int http_parser_is_finished(http_parser_t *parser);


#define http_parser_nread(parser) (parser)->nread 
}

#endif
