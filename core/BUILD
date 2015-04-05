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
        #"src/network_safe.cpp",
        "src/fd_ctx.cpp",
        "src/network_hook.cpp",
        "src/pthread_hook.cpp",
        #"src/disk_io_hook.cpp",
        "src/timewheel.cpp",
        "src/dispatch.cpp",
        "src/co_pool.cpp",
        "src/stacktrace.cpp",
        "src/jump_x86_64_sysv_elf_gas.s",
        "src/make_x86_64_sysv_elf_gas.s",
        "src/time_hook.cpp",
        "src/ares_wrap.cpp",
        "src/time_mgr.cpp",
        "src/common.cpp",
        #"src/malloc_hook.cpp",
    ],

    incs= [ 
        "./",
    ],

    deps=[
        '//base:base',
        '//thirdparty/gflags:gflags',
        '//thirdparty/glog:glog',
        '//thirdparty/c-ares:c-ares',
        '#aio',
        '#dl', 
        '#pthread',
        '#rt',
    ],

    extra_cppflags=["-fPIC"],
    export_incs=["./"],
    link_all_symbols = True,
    #export_dynamic = True,
);



cc_binary(
    name = 'co_swap_test',
    incs= [ 
        "./",
    ],
    srcs = ['src/co_swap_test.cpp'],
    deps = [
        ':conet',
    ],
);

cc_binary(
    name = 'gethostbyname_test',
    incs= [ 
        "./",
    ],
    srcs = ['src/gethostbyname_test.cpp'],
    deps = [
        ':conet',
    ],
);

