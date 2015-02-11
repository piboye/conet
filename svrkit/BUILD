cc_library(
     name = 'stat_mgr',
    srcs=[
        'src/stat_mgr.cpp',
    ],
    incs= [ 
        "./",
        "../base/",
        "../core/",
    ],
);

cc_library(
    name = 'udp_server',
    srcs = [
        "src/udp_server.cpp",
    ],
    incs= [ 
        "./",
        "../base/",
        "../core/",
    ],
    deps =[
        "//core:conet", 
        ':stat_mgr',
        "#rt",
    ],
    link_all_symbols = True,
);

cc_library(
    name = 'tcp_server',
    srcs = [
        "src/tcp_server.cpp",
    ],
    incs= [ 
        "./",
        "../base/",
        "../core/",
    ],
    deps =[
        "//core:conet", 
        ':stat_mgr',
        "#rt",
    ],
    link_all_symbols = True,
);

cc_library(
    name = 'client_base',
    srcs = [
        "src/rpc_pb_client.cpp",
        "src/rpc_pb_client_duplex.cpp",
    ],
    incs= [ 
        "./",
        "../base/",
        "../core/",
    ],
    deps =[
        "//core:conet", 
        ":rpc_base_pb_proto"
    ],
    link_all_symbols = True,
);


cc_library(
    name='http_server',
    srcs=[
        'src/http_server.cpp',
    ],
    incs= [ 
        "./",
        "../base/",
        "../core/",
    ],
    deps=[
        ':tcp_server',
    ],
);



proto_library(
    name='rpc_base_pb_proto',
    srcs=[
        'rpc_base_pb.proto'
    ],
);


resource_library(
    name = 'static_resource',
    srcs = [
        'static/list.html',
        'static/form.html',
        'static/jquery.js',
        'static/bootstrap.min.js',
        'static/bootstrap.min.css',
        'static/qrcode.min.js',
    ]
);

cc_library(
    name = 'rpc_pb_server',
    srcs = [
        "src/async_rpc_pb_server.cpp",
        "src/rpc_pb_server.cpp",
        "src/rpc_pb_server_base.cpp",
    ],
    incs= [ 
        "./",
        "../base/",
        "../core/",
    ],
    deps =[
        ":rpc_base_pb_proto",
        ":tcp_server",
        ":http_server",
        ":client_base",
        ':static_resource',
        "//thirdparty/jsoncpp:jsoncpp",
        "//thirdparty/protobuf:protobuf",
    ],
    link_all_symbols = True,
);

cc_library(
    name = 'server_main',
    srcs = [
        "src/server_main.cpp",
        "src/server_common.cpp",
        "src/server_controller.cpp",
        "src/server_worker.cpp",
        "src/server_task.cpp",
    ],
    incs= [ 
        "./",
        "../base/",
        "../core/",
    ],
    deps = [
        "//base:base",
    ],
    link_all_symbols = True,
);


cc_library(
    name = 'rpc_pb_server_main',
    srcs = [
        "src/rpc_pb_server_task.cpp",
    ],
    incs= [ 
        "./",
        "../base/",
        "../core/",
    ],
    deps =[
        ":server_main",
        ":rpc_pb_server",
    ],
    link_all_symbols = True,
);

cc_test(
    name = "pbc_test",
    srcs = [
        "src/rpc_base_pb_test.cpp"
    ],
    deps = [
        "//base:base",
        ":rpc_base_pb_proto",
    ],
)


cc_test(
    name = 'rpc_base_test',
    srcs = [
        "src/rpc_base_test.cpp",
    ],
    incs= [ 
        "./",
        "../base/",
        "../core/",
    ],
    deps =[
        ":rpc_base_pb_proto",
    ]
);



