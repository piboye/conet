#include "http_parser.h"
#include <stdio.h>
#include <assert.h>
#include <stdlib.h>
#include <ctype.h>
#include <string.h>

#define PTR_TO(F) (parser->F)

%%{
  machine http_request;
  
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

    init_ref_str(&parser->headers[num].name, parser->header_start, fpc);
  }
  
  action start_value { parser->header_val_start = fpc;}

  action write_value {
    int num = parser->headers_num;
    init_ref_str(&parser->headers[num].value, parser->header_val_start, fpc);
    ++parser->headers_num;
  }
  
  action http_accept {
    init_ref_str(&parser->accept, PTR_TO(mark), fpc);
  }
  action http_connection {
      if (*(parser->mark) == 'K') {
          parser->connection = CONNECTION_KEEPALIVE; 
      }
  }
  action http_content_length {
    parser->content_length = atoi(PTR_TO(mark));
  }
  
  action http_content_type {
    init_ref_str(&(parser->content_type), PTR_TO(mark),  fpc);
  }

### websocket 
  action upgrade {
    init_ref_str(&(parser->upgrade), PTR_TO(mark),  fpc);

    if (equal(parser->upgrade, ref_str("websocket")))
    {
        parser->websocket = 1;
    }
  }

  action sec_websocket_key {
    init_ref_str(&(parser->sec_websocket_key), PTR_TO(mark),  fpc);
  }

  action sec_websocket_version {
    init_ref_str(&(parser->sec_websocket_version), PTR_TO(mark),  fpc);
  }

  
  action fragment {
    init_ref_str(&(parser->fragment), PTR_TO(mark), fpc);
  }
  
  action http_version_1_0 {
      parser->version = HTTP_1_0;
  }

  action http_version_1_1 {
      parser->version = HTTP_1_1;
  }
  
  action request_path {
    init_ref_str(&parser->path, PTR_TO(mark), fpc);
  }
  
  action request_method_get { 
    parser->method = METHOD_GET;
  }

  action request_method_post { 
    parser->method = METHOD_POST;
  }
  
  action request_uri {
    init_ref_str(&parser->uri, PTR_TO(mark), fpc);
  }

  action request_host {
    init_ref_str(&parser->host, PTR_TO(mark), fpc);
  }
  
  action start_query {parser->query_start = fpc; }
  action query_string { 
    init_ref_str(&parser->query_string, PTR_TO(query_start), fpc);
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
  Method = ("GET"i %request_method_get | "POST"i %request_method_post );

  http_1_0 = "1.0";
  http_1_1 = "1.1";
  HTTP_Version = ( "HTTP/" (http_1_0 %http_version_1_0 | http_1_1 %http_version_1_1));

  Request_Line = ( Method " " Request_URI ("#" Fragment){0,1} " " HTTP_Version CRLF ) ;

  field_name = ( token -- ":" )+ >start_field %write_field;

  field_value = any* >start_value %write_value;
  
  known_header = ( ("Accept:"i         " "* (any* >mark %http_accept))
                 | ("Connection:"i     " "* (any* >mark %http_connection))
                 | ("Content-Length:"i " "* (digit+ >mark %http_content_length))
                 | ("Content-Type:"i   " "* (any* >mark %http_content_type))
                 | ("Host:"i   " "* (any* >mark %request_host))
                 | ("Upgrade:"i   " "* (any* >mark %upgrade))
                 | ("Sec-WebSocket-Key:"i   " "* (any* >mark %sec_websocket_key))
                 | ("Sec-WebSocket-Version:"i   " "* (any* >mark %sec_websocket_version))
                 ) :> CRLF;

  unknown_header = (field_name ":" " "* field_value :> CRLF) -- known_header;
  
  Request = Request_Line (known_header | unknown_header)* ( CRLF @done );

main := Request;

}%%

namespace conet
{

/** Data **/
%% write data;


void http_request_init(http_request_t *parser)  {
  int cs = 0;
  %% write init;

  memset(parser, 0, sizeof(*parser));
  parser->status = cs;
}


/** exec **/
size_t http_request_parse(http_request_t *parser, char *buffer, size_t len, size_t off)  {
  char *p, *pe;
  int cs = parser->status;
  
  if (off > len) return -1;
  
  p = buffer+off;
  pe = buffer+len;
  
  %% write exec;
  
  parser->status = cs;

  parser->nread += p - (buffer + off);
  
  return(parser->nread);
}

int http_request_finish(http_request_t *parser)
{
  if (http_request_has_error(parser))
    return -1;
  else if (http_request_is_finished(parser))
    return 1;
  else
    return 0;
}

int http_request_has_error(http_request_t *parser) {
  return parser->status == http_request_error || parser->err_too_many_header;
}

int http_request_is_finished(http_request_t *parser) {
  return parser->status >= http_request_first_final;
}

}
