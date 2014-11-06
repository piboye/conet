#gen_rule (
#    name = 'gen_co_context',
#    cmd = 
#    'gcc -c $SRCS.dir/coctx_swap.S -o coctx_swap.o;' +
#    'gcc -c $SRCS.dir/co_context.S -o co_context.o;' +
#          'ar -csr $SRCS.dir/libco_context.a co_context.o coctx_swap.o;' +
#          'cp $SRCS.dir/libco_context.a $SRCS.dir/../lib64_debug;' +
#          'cp $SRCS.dir/libco_context.a $SRCS.dir/../lib64_release;',
#    srcs = ['src/co_context.S'],
#    outs = ['libco_context.a'],
#);


cc_library (
    name = 'co_context',
    prebuilt=True,
    #deps = [':gen_co_context'],
);

cc_library(
    name = 'conet',
    srcs = [
        "src/version.cpp",
        "src/wait_queue.cpp",
        "src/coroutine.cpp",
        "src/coroutine_env.cpp",
        "src/gc.cpp",
        "src/network_safe.cpp",
        "src/fd_ctx.cpp",
        "src/network_hook.cpp",
        "src/pthread_hook.cpp",
        "src/disk_io_hook.cpp",
        "src/timewheel.cpp",
        "src/dispatch.cpp",
        "src/co_pool.cpp",
        "src/stacktrace.cpp",
        "src/coctx.cpp",
    ],

    incs= [ 
        "incl/",
    ],

    deps=[
        '//base:base',
        ':co_context',
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

