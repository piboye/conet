cc_binary(
    name = "echo_cli",
    srcs = ["echo_cli.cpp"],
    deps = ["//svrkit:client_base", "#dl", "#pthread"],
)

cc_binary(
    name = "echo_cli_co",
    srcs = ["echo_cli_co.cpp"],
    deps = [
            "//svrkit:client_base",
            "#dl", 
            #"//thirdparty/perftools:tcmalloc",
            "#pthread"
    ],
)

cc_binary(
    name = "echo_cli_co_async",
    srcs = ["echo_cli_co_async.cpp"],
    deps = ["//svrkit:client_base", "#dl", "#pthread"],
)

cc_binary(
    name = "echo_rpc_cli",
    srcs = ["echo_rpc_cli.cpp"],
    deps = [
            ":echo_rpc_proto", 
            "//svrkit:client_base", 
            #"//thirdparty/perftools:tcmalloc_debug",
            "#dl", 
            "#pthread",
           ],
)

cc_binary(
    name = "echo_rpc_cli_udp",
    srcs = ["echo_rpc_cli_udp.cpp"],
    deps = [
            ":echo_rpc_proto", 
            "//svrkit:client_base", 
            #"//thirdparty/perftools:tcmalloc_debug",
            "#dl", 
            "#pthread",
           ],
)

cc_binary(
    name = "echo_rpc_cli_duplex2",
    srcs = ["echo_rpc_cli_duplex2.cpp"],
    deps = [
            ":echo_rpc_proto", 
            "//svrkit:client_base", 
            #"//thirdparty/perftools:tcmalloc_debug",
            #"//thirdparty/perftools:profiler",
            "#dl", "#pthread"],
)

cc_binary(
    name = "echo_rpc_cli_duplex",
    srcs = ["echo_rpc_cli_duplex.cpp"],
    deps = [
            ":echo_rpc_proto", 
            "//svrkit:client_base", 
            #"//thirdparty/perftools:tcmalloc_debug",
            #"//thirdparty/perftools:profiler",
            "#dl", "#pthread"],
)

cc_binary(
    name = "echo_rpc_cli_co",
    srcs = ["echo_rpc_cli_co.cpp"],
    deps = [
            ":echo_rpc_proto", 
            "//svrkit:client_base", 
            #"//thirdparty/perftools:tcmalloc",
            #"//thirdparty/perftools:profiler",
            "#dl", "#pthread"],
)

cc_binary(
    name = "echo_rpc_cli_co2",
    srcs = ["echo_rpc_cli_co2.cpp"],
    deps = [
            ":echo_rpc_proto", 
            "//svrkit:client_base", 
            #"//thirdparty/perftools:tcmalloc",
            #"//thirdparty/perftools:profiler",
            "#dl", "#pthread"],
)

proto_library(
        name='echo_rpc_proto',
        srcs=[
            'echo_rpc.proto'
        ],
)

cc_binary(
    name = "echo_server",
    srcs = ["echo_server.cpp"],
    deps = [
        "//svrkit:tcp_server", 
        "#dl", 
        #"//thirdparty/perftools:tcmalloc",
        "#pthread"
    ],
)

cc_binary(
    name = "echo_rpc_server",
    srcs = ["echo_rpc_server.cpp"],
    deps = [":echo_rpc_proto",
            "//svrkit:rpc_pb_server_main", 
            #"//thirdparty/perftools:tcmalloc",
            #"//thirdparty/perftools:profiler",
            "//thirdparty/gflags:gflags",
            "#dl", "#pthread", 
           ],
    #link_all_symbols = True,
)

cc_binary(
    name = "http_server",
    srcs = ["http_server.cpp"],
    deps = [
            "//svrkit:http_server", 
            #"//thirdparty/perftools:tcmalloc_debug",
            #"//thirdparty/perftools:profiler",
            "//thirdparty/gflags:gflags",
            "#dl", "#pthread", 
           ],
)


cc_binary(
    name = "aio_test",
    srcs = ["aio_test.cpp"],
    deps = [
        "//core:conet", 
        #'//thirdparty/perftools:tcmalloc_debug',
        "#dl", 
        "#pthread"
    ],
)
