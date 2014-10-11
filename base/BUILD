gen_rule(
    name='http_parser_rl',
    srcs=['src/http_parser.rl'],
    cmd='ragel $SRCS -o $OUTS',
    outs=['src/http_parser.cpp'],
);

cc_library(
    name="base",
    srcs = [
        'src/pb2sqlite.cpp',
        'src/net_tool.cpp',
        'src/tls.cpp',
        'src/time_helper.cpp',
        'src/query_string.cpp',
        'src/http_parser.cpp',
        'src/url_encode.cpp',
        'src/bobhash.cpp',
        'src/delay_init.cpp',
        'src/pb2json.cpp',
        'src/pbc.cpp',
    ],

    incs= [ 
        "incl/",
    ],

    deps = [
        ":http_parser_rl",
    ],
);

cc_test(
    name='base_test',
    srcs =[
        'src/http_parser_test.cpp',
        'src/query_string_test.cpp',
    ],

    incs= [ 
        "incl/",
    ],

    deps=[
        ":base",
        "//thirdparty/jsoncpp:jsoncpp",
         ],
);

