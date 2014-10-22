##gen_rule (
##    name = 'gen_co_context',
##    cmd = 'gcc -c $SRCS -o co_context.o;' +
##          'ar -csr $SRCS.dir/libco_context.a co_context.o;' +
##          'cp $SRCS.dir/libco_context.a $SRCS.dir/../lib64_debug;' +
##          'cp $SRCS.dir/libco_context.a $SRCS.dir/../lib64_release;',
##    srcs = ['src/co_context.S'],
##    outs = ['libco_context.a'],
##);


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
        ':co_context',
        '//thirdparty/gflags:gflags',
        '//thirdparty/glog:glog',
        '#aio',
        '#dl', 
        '#pthread',
        '#rt',
        "#unwind", 
        "#unwind-x86_64",
    ],

    extra_cppflags=["-fPIC"],

    export_incs=["incl/"],
    link_all_symbols = True,
);

