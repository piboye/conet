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
        'src/random.cpp',
        'src/pb_obj_pool.cpp',
        'src/cpu_affinity.cpp',
        'src/fixed_mempool.cpp',
        'src/unix_socket_send_fd.cpp',
        'src/func_wrap_mgr.cpp',
        'src/thumb.s',
        'src/fd_queue.cpp',
    ],

    incs= [ 
        "./",
    ],

    extra_cppflags=["-fPIC"],

    deps = [
        ":http_parser_rl",
        ":query_string_rl",
        "//thirdparty/jsoncpp:jsoncpp",
        "//thirdparty/protobuf:protobuf",
        "//thirdparty/sqlite:sqlite3",
        "//thirdparty/glog:glog",
    ],
);

cc_test(
    name='base_test',
    srcs =[
        'src/http_parser_test.cpp',
        'src/query_string_test.cpp',
    ],

    incs= [ 
        "./",
    ],


    deps=[
        ":base",
        "//thirdparty/jsoncpp:jsoncpp",
         ],
);


cc_test(
    name='defer_test',
    srcs=[
        "src/defer_test.cpp",
    ],
    incs=[
        "./",
    ],
);

cc_binary(
    name = 'tls_bench',
    srcs = [
        'src/tls_bench.cpp',
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
    name = 'time_helper_test',
    srcs = [
        'src/time_helper_test.cpp',
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
    name = 'ptr_cast_test',
    srcs = [
        'src/ptr_cast_test.cpp',
    ],

    incs= [ 
        "./",
    ],

    deps = [
    ],
) 

proto_library(
    name = 'test_proto',
    srcs = [
        'src/test.proto',
    ],
)

cc_binary(
    name = 'pb2sqlite_test',

    srcs = [
        'src/pb2sqlite_test.cpp',
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

cc_test(
    name = 'functor_test',
    srcs = [
        'src/functor_test.cpp',
    ],

    incs= [ 
        "./",
    ],
)

cc_binary(
    name = 'func_wrap_test',
    srcs = [
        'src/func_wrap_test.cpp',
    ],

    incs= [ 
        "./",
    ],
    deps=[":base"],
)
