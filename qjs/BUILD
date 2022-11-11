import warnings

cc_library(
    name = 'qjs',
    srcs = [
        'quickjs-libc.c',
        'cutils.c',
        'quickjs.c',
        'libregexp.c',
        'libunicode.c',
        'libbf.c'
    ],

    incs= [ 
        "./",
    ],

    deps=[
    ],
    
    extra_cppflags=['-Wno-error', "-Wno-array-bounds", "-Wno-format-truncation"],
    defs=['_GNU_SOURCE', 'CONFIG_BIGNUM', 'CONFIG_CHECK_JSVALUE', 'CONFIG_VERSION=\\"_2021-03-27\\"'],

    export_incs=["./"],
    warning = 'no',
    link_all_symbols = True,
    #export_dynamic = True,
);
