cc_library(
    name = 'server_base',
    srcs = [
        "server_base.cpp",
        "co_pool.cpp",
        "sigsegv.cpp",
    ],
    deps =[
        "//core:conet", 
        "#unwind", 
        "#unwind-x86_64"
    ],
    link_all_symbols = True,
);

cc_library(
    name = 'client_base',
    srcs = [
        "rpc_pb_client.cpp",
    ],
    deps =[
        "//core:conet", 
        ":rpc_base_pb_proto"
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
        ":client_base",
    ],
    link_all_symbols = True,
);

