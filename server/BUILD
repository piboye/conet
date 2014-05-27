cc_library(
    name = 'server_base',
    srcs = [
        "server_base.cpp",
        "sigsegv.cpp",
    ],
    deps =[
        "//core:conet"
    ],
    link_all_symbols = True,
);


proto_library(
    name='rpc_base_pb_proto',
    srcs=[
        'rpc_base_pb.proto'
    ],
)

cc_library(
    name = 'rpc_pb_server',
    srcs = [
        "rpc_pb_server.cpp"
    ],
    deps =[
        ":rpc_base_pb_proto",
        ":server_base",
    ],
    link_all_symbols = True,
);

