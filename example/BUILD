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
