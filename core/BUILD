cc_library(
    name = 'conet',
    srcs = [
        'src/swap_context.s',
        "src/version.cpp",
        "src/wait_queue.cpp",
        "src/coroutine.cpp",
        "src/coroutine_env.cpp",
        "src/gc.cpp",
        "src/network.cpp",
        "src/fd_ctx.cpp",
        "src/network_hook.cpp",
        "src/pthread_hook.cpp",
        "src/disk_io_hook.cpp",
        "src/timewheel.cpp",
        "src/dispatch.cpp",
        "src/co_pool.cpp",
        "src/stacktrace.cpp",
    ],

    incs= [ 
        "incl/",
    ],

    deps=[
        '//base:base',
        '//thirdparty/gflags:gflags',
        '//thirdparty/glog:glog',
        '#aio',
        '#dl', 
        '#pthread',
        '#rt',
        '#lzma',
        "//thirdparty/libunwind:unwind-setjmp", 
        "//thirdparty/libunwind:unwind-ptrace",
        "//thirdparty/libunwind:unwind-x86_64",
        "//thirdparty/libunwind:unwind", 
    ],

    extra_cppflags=["-fPIC"],

    export_incs=["incl/"],
    link_all_symbols = True,
);



cc_binary(
    name = 'co_swap_test',
    srcs = ['src/co_swap_test.cpp'],
    deps = [
        ':conet',
    ],
);

