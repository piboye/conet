import warnings


gen_rule(
    name='http_parser_rl',
    srcs=['src/http_parser.rl'],
    cmd='ragel $SRCS -o $OUTS',
    outs=['src/http_parser.cpp'],
);

gen_rule(
    name='query_string_rl',
    srcs=['src/query_string.rl'],
    cmd='ragel $SRCS -o $OUTS',
    outs=['src/query_string.cpp'],
);

cc_library(
    name='http_tool',
    srcs=[
        'src/http_parser.cpp',
        'src/query_string.cpp',
        'src/url_encode.cpp',
    ],
    incs= [ 
        "./",
    ],
    extra_cppflags=['-Wno-unused-variable'],
    deps = [
        ":http_parser_rl",
        ":query_string_rl",
    ]
);

cc_library(
    name="base",
    srcs = [
        'src/plog.cpp',
        'src/module.cpp',
        'src/pb2sqlite.cpp',
        'src/net_tool.cpp',
        'src/tls.cpp',
        'src/time_helper.cpp',
        'src/bobhash.cpp',
        'src/pb2json.cpp',
        'src/pbc.cpp',
        'src/murmurhash3.cpp',
        'src/random.cpp',
        'src/pb_obj_pool.cpp',
        'src/cpu_affinity.cpp',
        'src/fixed_mempool.cpp',
        'src/unix_socket_send_fd.cpp',
        'src/bind_this_mgr.cpp',
        'src/bind_this_jump.s',
        'src/fd_queue.cpp',
        'src/base64.cpp',
        'src/ebpf.c',
    ],

    incs= [ 
        "./",
    ],

    extra_cppflags=["-fPIC"],

    deps = [
        ":http_tool",
        "//thirdparty/jsoncpp:jsoncpp",
        "//thirdparty/protobuf:protobuf",
        "//thirdparty/sqlite:sqlite3",
    ],
);



cc_test(
    name='base_test',
    srcs =[
        'test/bind_this_test.cpp',
        'test/http_parser_test.cpp',
        'test/query_string_test.cpp',
        "test/defer_test.cpp",
        'test/to_json_test.cpp',
        'test/json_to_test.cpp',
        'test/to_xml_test.cpp',
        'test/ptr_cast_test.cpp',
        'test/closure_test.cpp',
        'test/url_encode_test.cpp',
        'test/string_tpl_test.cpp',
    ],

    incs= [
        "./",
    ],


    deps=[
        ":base",
        "//thirdparty/jsoncpp:jsoncpp",
         ],
);

cc_binary(
    name='module_test',
    srcs=[
        "test/module_a.cpp",
        "test/module_b.cpp",
        "test/module_test.cpp",
    ],
    incs=[
        "../",
    ],
    deps = [
        ":base",
    ]
);


cc_binary(
    name = 'plog_test',
    srcs = [
        'test/plog_test.cpp',
    ],

    incs= [
        "./",
    ],

    deps = [
        ":base",
        "//thirdparty/gflags:gflags",
    ],
) 


cc_binary(
    name = 'tls_bench',
    srcs = [
        'test/tls_bench.cpp',
    ],

    incs= [ 
        "./",
    ],

    deps = [
        ":base",
        "//thirdparty/gflags:gflags",
    ],
) 

proto_library(
    name = 'test_proto',
    srcs = [
        'test/test.proto',
    ],
)

cc_binary(
    name = 'pb2sqlite_test',

    srcs = [
        'test/pb2sqlite_test.cpp',
    ],

    incs= [ 
        "./",
    ],

    deps = [
        ':base',
        ':test_proto',
        '#dl',
    ],
) 

cc_binary(
    name = 'time_helper_test',
    srcs = [
        'test/time_helper_test.cpp',
    ],

    incs= [ 
        "./",
    ],

    deps = [
        ':base',
        '#dl',
    ],
)

