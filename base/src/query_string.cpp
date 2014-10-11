
#line 1 "query_string.rl"
#include <map>
#include <string>
#include <assert.h>
#include <stdlib.h>
#include <ctype.h>
#include <string.h>

#include "query_string.h"
#include "url_encode.h"


#line 36 "query_string.rl"


namespace conet
{

/** Data **/

#line 23 "query_string.cpp"
static const char _query_string_actions[] = {
	0, 1, 0, 1, 1, 1, 2, 1, 
	3, 2, 1, 0, 2, 1, 2, 2, 
	2, 3, 2, 3, 0
};

static const char _query_string_key_offsets[] = {
	0, 1, 2, 3, 4, 6, 8
};

static const char _query_string_trans_keys[] = {
	61, 61, 61, 38, 38, 61, 38, 61, 
	38, 61, 0
};

static const char _query_string_single_lengths[] = {
	1, 1, 1, 1, 2, 2, 2
};

static const char _query_string_range_lengths[] = {
	0, 0, 0, 0, 0, 0, 0
};

static const char _query_string_index_offsets[] = {
	0, 2, 4, 6, 8, 11, 14
};

static const char _query_string_trans_targs[] = {
	1, 0, 6, 4, 1, 0, 2, 0, 
	5, 6, 4, 5, 6, 4, 5, 6, 
	4, 0
};

static const char _query_string_trans_actions[] = {
	3, 0, 12, 5, 9, 1, 1, 1, 
	7, 3, 0, 18, 9, 1, 15, 12, 
	5, 0
};

static const char _query_string_eof_actions[] = {
	0, 0, 0, 0, 7, 7, 7
};

static const int query_string_start = 3;
static const int query_string_first_final = 3;
static const int query_string_error = -1;

static const int query_string_en_main = 3;


#line 43 "query_string.rl"

int parse_query_string(char const *buf, size_t len, std::map<std::string, std::string> *param)
{
    int cs = 0;
    
#line 80 "query_string.cpp"
	{
	cs = query_string_start;
	}

#line 48 "query_string.rl"

    char const *p, *pe;
    char const *mark=NULL;

    p = buf;
    pe = buf + len;
    const char* eof = pe;

    std::string name;
    std::string value, tmp;

    
#line 98 "query_string.cpp"
	{
	int _klen;
	unsigned int _trans;
	const char *_acts;
	unsigned int _nacts;
	const char *_keys;

	if ( p == pe )
		goto _test_eof;
_resume:
	_keys = _query_string_trans_keys + _query_string_key_offsets[cs];
	_trans = _query_string_index_offsets[cs];

	_klen = _query_string_single_lengths[cs];
	if ( _klen > 0 ) {
		const char *_lower = _keys;
		const char *_mid;
		const char *_upper = _keys + _klen - 1;
		while (1) {
			if ( _upper < _lower )
				break;

			_mid = _lower + ((_upper-_lower) >> 1);
			if ( (*p) < *_mid )
				_upper = _mid - 1;
			else if ( (*p) > *_mid )
				_lower = _mid + 1;
			else {
				_trans += (unsigned int)(_mid - _keys);
				goto _match;
			}
		}
		_keys += _klen;
		_trans += _klen;
	}

	_klen = _query_string_range_lengths[cs];
	if ( _klen > 0 ) {
		const char *_lower = _keys;
		const char *_mid;
		const char *_upper = _keys + (_klen<<1) - 2;
		while (1) {
			if ( _upper < _lower )
				break;

			_mid = _lower + (((_upper-_lower) >> 1) & ~1);
			if ( (*p) < _mid[0] )
				_upper = _mid - 2;
			else if ( (*p) > _mid[1] )
				_lower = _mid + 2;
			else {
				_trans += (unsigned int)((_mid - _keys)>>1);
				goto _match;
			}
		}
		_trans += _klen;
	}

_match:
	cs = _query_string_trans_targs[_trans];

	if ( _query_string_trans_actions[_trans] == 0 )
		goto _again;

	_acts = _query_string_actions + _query_string_trans_actions[_trans];
	_nacts = (unsigned int) *_acts++;
	while ( _nacts-- > 0 )
	{
		switch ( *_acts++ )
		{
	case 0:
#line 14 "query_string.rl"
	{
      mark = p;
  }
	break;
	case 1:
#line 18 "query_string.rl"
	{
      name.assign(mark, p-mark); 
  }
	break;
	case 2:
#line 22 "query_string.rl"
	{
      mark = p;
  }
	break;
	case 3:
#line 25 "query_string.rl"
	{
      tmp.assign(mark, p-mark); 
      value.clear();
      url_decode(tmp, &value);
      param->insert(std::make_pair(name, value));
  }
	break;
#line 196 "query_string.cpp"
		}
	}

_again:
	if ( ++p != pe )
		goto _resume;
	_test_eof: {}
	if ( p == eof )
	{
	const char *__acts = _query_string_actions + _query_string_eof_actions[cs];
	unsigned int __nacts = (unsigned int) *__acts++;
	while ( __nacts-- > 0 ) {
		switch ( *__acts++ ) {
	case 3:
#line 25 "query_string.rl"
	{
      tmp.assign(mark, p-mark); 
      value.clear();
      url_decode(tmp, &value);
      param->insert(std::make_pair(name, value));
  }
	break;
#line 219 "query_string.cpp"
		}
	}
	}

	}

#line 60 "query_string.rl"

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

