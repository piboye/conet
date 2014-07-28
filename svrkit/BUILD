cc_library(
    name = 'server_base',
    srcs = [
        "server_base.cpp",
        "co_pool.cpp",
        #"sigsegv.cpp",
    ],
    deps =[
        "//core:conet", 
        #"#unwind", 
        #"#unwind-x86_64"
        "//thirdparty/libunwind:unwind",
    ],
    link_all_symbols = True,
);

cc_library(
    name = 'client_base',
    srcs = [
        "rpc_pb_client.cpp",
        "bobhash.cpp",
    ],
    deps =[
        "//core:conet", 
        ":rpc_base_pb_proto"
    ],
    link_all_symbols = True,
);

gen_rule(
    name='http_parser_rl',
    srcs=['http_parser.rl'],
    cmd='ragel $SRCS -o $OUTS',
    outs=['http_parser.cpp'],
);

cc_library(
    name='http_parser',
    srcs=[
        'http_parser.cpp',
    ],
    deps=[':http_parser_rl'],
);

cc_library(
    name='http_server',
    srcs=[
        'http_server.cpp',
    ],
    deps=[
        ':http_parser',
        ':server_base',
    ],
);



proto_library(
    name='rpc_base_pb_proto',
    srcs=[
        'rpc_base_pb.proto'
    ],
);

cc_library(
    name = 'rpc_pb_server',
    srcs = [
        "rpc_pb_server.cpp",
        "pb2json.cpp",
        "query_string.cpp",
        "url_encode.cpp",
    ],
    deps =[
        ":rpc_base_pb_proto",
        ":server_base",
        ":http_server",
        ":client_base",
        "//thirdparty/jsoncpp:jsoncpp",
    ],
    link_all_symbols = True,
);

cc_test(
    name='test',
    srcs =[
        'http_parser_test.cpp',
        'query_string_test.cpp',
        'query_string.cpp',
        "url_encode.cpp",
    ],
    deps=[':http_parser',
        "//thirdparty/jsoncpp:jsoncpp",
         ],
);


