cc_library(
    name = 'server_base',
    srcs = [
        "src/server_base.cpp",
    ],
    incs= [ 
        "incl/",
    ],
    deps =[
        "//core:conet", 
        #"#unwind", 
        #"#unwind-x86_64",
        #"//thirdparty/libunwind:unwind-x86_64",
        #"//thirdparty/libunwind:unwind",
        #"//thirdparty/libunwind:unwind-elf64",
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
        "incl/",
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
        "incl/",
    ],
    deps=[
        ':server_base',
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
        "src/rpc_pb_server.cpp",
    ],
    incs= [ 
        "incl/",
    ],
    deps =[
        ":rpc_base_pb_proto",
        ":server_base",
        ":http_server",
        ":client_base",
        ':static_resource',
        "//thirdparty/jsoncpp:jsoncpp",
        "//thirdparty/protobuf:protobuf",
    ],
    link_all_symbols = True,
);

cc_library(
    name = 'rpc_pb_server_main',
    srcs = [
        "src/rpc_pb_server_main.cpp",
    ],
    incs= [ 
        "incl/",
    ],
    deps =[
        ":rpc_pb_server",
        #"//thirdparty/perftools:tcmalloc",
    ]
);

cc_test(
    name = 'rpc_base_test',
    srcs = [
        "src/rpc_base_test.cpp",
    ],
    incs= [ 
        "incl/",
    ],
    deps =[
        ":rpc_base_pb_proto",
    ]
);



