#include <map>
#include <string>
#include <assert.h>
#include <stdlib.h>
#include <ctype.h>
#include <string.h>

#include "query_string.h"
#include "url_encode.h"

%%{
  machine query_string;

  action start_name {
      mark = fpc;
  }

  action end_name {
      name.assign(mark, fpc-mark); 
  }

  action start_value {
      mark = fpc;
  }
  action end_value {
      tmp.assign(mark, fpc-mark); 
      value.clear();
      url_decode(tmp, &value);
      param->insert(std::make_pair(name, value));
  }

  name = (any -- "=&")+ >start_name %end_name;
  value = (any -- "=&")+ >start_value %end_value;
  main := (name "=" value)?("&" name "=" value)*;

}%%

namespace conet
{

/** Data **/
%% write data;

int parse_query_string(char const *buf, size_t len, std::map<std::string, std::string> *param)
{
    int cs = 0;
    %% write init;

    char const *p, *pe;
    char const *mark=NULL;

    p = buf;
    pe = buf + len;
    const char* eof = pe;

    std::string name;
    std::string value, tmp;

    %% write exec;

    if (cs == query_string_error || cs <query_string_first_final) {
        return -1;
    }
    return 0;
}


int query_string_to_json(char const *buf, size_t len, Json::Value *root)
{
    std::map<std::string, std::string> params;
    int ret = 0;
    ret = parse_query_string(buf, len, &params);
    if (ret) {
        return ret;
    }

    for(typeof(params.begin()) it = params.begin(); it != params.end(); ++it)
    {
        (*root)[it->first.c_str()]=it->second;
    }
    return 0;
}

}

