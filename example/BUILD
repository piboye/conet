cc_binary(
    name = "echo_server",
    srcs = ["echo_server.cpp"],
    deps = ["//server:server_base", "#dl", "#pthread"],
)

cc_binary(
    name = "echo_cli",
    srcs = ["echo_cli.cpp"],
    deps = ["//server:server_base", "#dl", "#pthread"],
)

cc_binary(
    name = "echo_cli_co",
    srcs = ["echo_cli_co.cpp"],
    deps = ["//server:server_base", "#dl", "#pthread"],
)

proto_library(
        name='echo_rpc_proto',
        srcs=[
            'echo_rpc.proto'
        ],
        export_incs=["example"],
)

cc_binary(
    name = "echo_rpc_server",
    srcs = ["echo_rpc_server.cpp"],
    deps = [":echo_rpc_proto", "//server:rpc_pb_server", "#dl", "#pthread"],
)

cc_binary(
    name = "echo_rpc_cli",
    srcs = ["echo_rpc_cli.cpp"],
    deps = [":echo_rpc_proto", "//server:rpc_pb_server", "#dl", "#pthread"],
)
