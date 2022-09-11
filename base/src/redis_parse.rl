#include <stdio.h>
#include <string.h>
#include <stdbool.h>
#include <string_view>
#include "redis_parse.h"

static int cs;

%%{
  machine RedisParser;

  action argnum_add_digit { sc->argnum = sc->argnum * 10 + (fc-'0'); }
  action argsize_reset { sc->cur_arg_size = 0; }
  action argsize_add_digit { sc->cur_arg_size = sc->cur_arg_size * 10 + (fc-'0'); }
  action args_init {
    sc->cur_arg_pos = -1;
    sc->args = (std::string_view *)sc->_reserve;
  }

  action arg_init {
    sc->cur_arg = &sc->args[++sc->cur_arg_pos];
    sc->cur_arg_p = fpc+1;
  }

  action end_arg {
    *sc->cur_arg = std::string_view(sc->cur_arg_p,  fpc-1-sc->cur_arg_p);
    if (sc->cur_arg_pos >= sc->argnum) {
      cs = RedisParser_first_final;
    }
  }

  action do_set_cmd{
    sc->cmd = redis_parser_t::SET;
    sc->argnum = 2;
  }

  action do_get_cmd{
    sc->cmd = redis_parser_t::GET;
    sc->argnum = 1;
  }

  redis_argnum = '*' ( digit @argnum_add_digit )+ '\r\n';
  redis_argsize = '$' @argsize_reset ( digit @argsize_add_digit )+ '\r\n';

  redis_arg_body = (any+)'\r\n' @end_arg;

  redis_args = (redis_argsize @arg_init redis_arg_body)+;
  get_cmd = '$3\r\nget\r\n' @do_get_cmd redis_args;
  set_cmd = '$3\r\nset\r\n' @do_set_cmd redis_args;
  redis_cmd = get_cmd | set_cmd;
  redis_expr = redis_argnum @args_init (redis_cmd);

  main := redis_expr;
}%%

%% write data;

void redis_parser_t::init() {
  redis_parser_t * sc = this;
  sc->argnum = 0;
  sc->cmd = 0;
  sc->cur_arg_pos = 0;
  %% write init;
}

int redis_parser_exec(redis_parser_t *sc, const char *data, int len) {
  const char *p = data;
  const char *pe = data + len;
  %% write exec;
  if (cs == RedisParser_error) return -1;
  else if (cs >= RedisParser_first_final) return 1;
  else return 0;
}

int redis_parser_finish(redis_parser_t *sc) {
  if (cs == RedisParser_error) return -1;
  else if (cs >= RedisParser_first_final) return 1;
  else return 0;
}
