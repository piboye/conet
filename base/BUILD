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
        'src/murmurhash3.cpp',
    ],

    incs= [ 
        "incl/",
    ],

    extra_cppflags=["-fPIC"],

    deps = [
        ":http_parser_rl",
        "//thirdparty/jsoncpp:jsoncpp",
        "//thirdparty/protobuf:protobuf",
        "//thirdparty/sqlite:sqlite3",
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

