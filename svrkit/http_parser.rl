#include "svrkit/http_parser.h"
#include <stdio.h>
#include <assert.h>
#include <stdlib.h>
#include <ctype.h>
#include <string.h>

#define TRUE 1
#define FALSE 0

#define LEN(AT, FPC) (FPC - parser->AT)
#define MARK(M,FPC) (parser->M = FPC)
#define PTR_TO(F) (parser->F)


/** machine **/
%%{
  machine http_parser;
  
  action mark {
      parser->mark = fpc;
  }
  
  action start_field { parser->header_start = fpc; }
  action write_field { 
    int num = parser->headers_num;
    if(num >= 100) {
      parser->err_too_many_header = 1;
      fbreak;
    }

    init_ref_str(&(parser->headers[num].name), 
            parser->header_start, 
            LEN(header_start, fpc));
  }
  
  action start_value { parser->header_val_start = fpc;}

  action write_value {
    int num = parser->headers_num;
    init_ref_str(&(parser->headers[num].value), 
                parser->header_val_start, 
                LEN(header_val_start, fpc));
    ++parser->headers_num;
  }
  
  action http_accept {
    init_ref_str(&parser->accept, PTR_TO(mark), LEN(mark, fpc));
  }
  action http_connection {
    init_ref_str(&parser->connection, PTR_TO(mark), LEN(mark, fpc));
  }
  action http_content_length {
    parser->content_length = atoi(PTR_TO(mark));
  }
  
  action http_content_type {
    init_ref_str(&(parser->content_type), PTR_TO(mark), LEN(mark, fpc));
  }
  
  action fragment {
    init_ref_str(&(parser->fragment), PTR_TO(mark), LEN(mark, fpc));
  }
  
  action http_version {
    init_ref_str(&parser->version, PTR_TO(mark), LEN(mark, fpc));
  }
  
  action request_path {
    init_ref_str(&parser->path, PTR_TO(mark), LEN(mark, fpc));
  }
  
  action request_method { 
    init_ref_str(&parser->method, PTR_TO(mark), LEN(mark, fpc));
  }
  
  action request_uri {
    init_ref_str(&parser->uri, PTR_TO(mark), LEN(mark, fpc));
  }
  
  action start_query {MARK(query_start, fpc); }
  action query_string { 
    init_ref_str(&parser->query_string, PTR_TO(query_start), LEN(query_start, fpc));
  }
  
  
  action done {
    parser->body = fpc+1;
    fbreak;
  }

#### HTTP PROTOCOL GRAMMAR
# line endings
  CRLF = "\r\n";

# character types
  CTL = (cntrl | 127);
  safe = ("$" | "-" | "_" | ".");
  extra = ("!" | "*" | "'" | "(" | ")" | ",");
  reserved = (";" | "/" | "?" | ":" | "@" | "&" | "=" | "+");
  unsafe = (CTL | " " | "\"" | "#" | "%" | "<" | ">");
  national = any -- (alpha | digit | reserved | extra | safe | unsafe);
  unreserved = (alpha | digit | safe | extra | national);
  escape = ("%" xdigit xdigit);
  uchar = (unreserved | escape);
  pchar = (uchar | ":" | "@" | "&" | "=" | "+");
  tspecials = ("(" | ")" | "<" | ">" | "@" | "," | ";" | ":" | "\\" | "\"" | "/" | "[" | "]" | "?" | "=" | "{" | "}" | " " | "\t");
  hval = any -- ("\r" | "\n");

# elements
  token = (ascii -- (CTL | tspecials));

# URI schemes and absolute paths
  scheme = ( alpha | digit | "+" | "-" | "." )* ;
  absolute_uri = (scheme ":" (uchar | reserved )*);

  path = ( pchar+ ( "/" pchar* )* ) ;
  query = ( uchar | reserved )* %query_string ;
  param = ( pchar | "/" )* ;
  params = ( param ( ";" param )* ) ;
  rel_path = ( path? %request_path (";" params)? ) ("?" %start_query query)?;
  absolute_path = ( "/"+ rel_path );

  Request_URI = ( "*" | absolute_uri | absolute_path ) >mark %request_uri;
  Fragment = ( uchar | reserved )* >mark %fragment;
  Method = ( upper | digit | safe ){1,20} >mark %request_method;

  http_number = ( digit+ "." digit+ ) ;
  HTTP_Version = ( "HTTP/" http_number ) >mark %http_version ;
  Request_Line = ( Method " " Request_URI ("#" Fragment){0,1} " " HTTP_Version CRLF ) ;

  field_name = ( token -- ":" )+ >start_field %write_field;

  field_value = hval* >start_value %write_value;
  
  known_header = ( ("Accept:"i         " "* (hval* >mark %http_accept))
                 | ("Connection:"i     " "* (hval* >mark %http_connection))
                 | ("Content-Length:"i " "* (digit+ >mark %http_content_length))
                 | ("Content-Type:"i   " "* (hval* >mark %http_content_type))
                 ) :> CRLF;

  unknown_header = (field_name ":" " "* field_value :> CRLF) -- known_header;
  
  Request = Request_Line (known_header | unknown_header)* ( CRLF @done );

main := Request;

}%%

namespace conet
{

/** Data **/
%% write data;


void http_parser_init(http_parser_t *parser)  {
  int cs = 0;
  %% write init;

  memset(parser, 0, sizeof(*parser));
  parser->status = cs;
}


/** exec **/
size_t http_parser_execute(http_parser_t *parser, char *buffer, size_t len, size_t off)  {
  char *p, *pe;
  int cs = parser->status;
  
  assert(off <= len && "offset past end of buffer");
  
  p = buffer+off;
  pe = buffer+len;
  
  %% write exec;
  
  parser->status = cs;

  parser->nread += p - (buffer + off);
  
  return(parser->nread);
}

int http_parser_finish(http_parser_t *parser)
{
  if (http_parser_has_error(parser))
    return -1;
  else if (http_parser_is_finished(parser))
    return 1;
  else
    return 0;
}

int http_parser_has_error(http_parser_t *parser) {
  return parser->status == http_parser_error || parser->err_too_many_header;
}

int http_parser_is_finished(http_parser_t *parser) {
  return parser->status >= http_parser_first_final;
}

}
