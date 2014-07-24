cc_library(
    name = 'conet',
    srcs = [
        "src/coroutine.cpp",
        "src/coroutine_env.cpp",
        "src/gc.cpp",
        "src/network.cpp",
        "src/fd_ctx.cpp",
        "src/network_hook.cpp",
        "src/pthread_hook.cpp",
        #"src/disk_io_hook.cpp",
        "src/timewheel.cpp",
        "src/dispatch.cpp",
        "src/tls.cpp",
        "src/time_helper.cpp",
        "src/net_tool.cpp",
    ],
    incs= [ 
        "incl/",
    ],
    deps=[
        '//thirdparty/gflags:gflags',
        "//thirdparty/glog:glog",
        '#aio','#dl', '#pthread'
    ],
    warning=["fPIC"],
    export_incs=["incl/"],
    link_all_symbols = True,
);
