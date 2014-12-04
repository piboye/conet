/*
 * =====================================================================================
 *
 *       Filename:  stacktrace.cpp
 *
 *    Description:  
 *
 *        Version:  1.0
 *        Created:  2014年10月15日 21时33分04秒
 *       Revision:  none
 *       Compiler:  gcc
 *
 *         Author:  piboye
 *   Organization:  
 *
 * =====================================================================================
 */
#include <memory.h>
#include <stdlib.h>
#include <stdio.h>
#include <unistd.h>
#include <signal.h>
#include <ucontext.h>
#include <dlfcn.h>
#include <cxxabi.h>
#include <execinfo.h>
#include "libunwind/libunwind.h"
#include <assert.h>
#include "coroutine_impl.h"

using __cxxabiv1::__cxa_demangle;

static const char * g_regs_name[NGREG] =
{
    "GS",
    "FS",
    "ES",
    "DS",
    "EDI",
    "ESI",
    "EBP",
    "ESP",
    "EBX",
    "EDX",
    "ECX",
    "EAX",
    "TRAPNO",
    "ERR",
    "EIP",
    "CS",
    "EFL",
    "UESP",
    "SS"
};

#define REGFORMAT "%016lx"

namespace conet
{
void print_ucontext(ucontext_t *a_uc, int fd)
{
    unw_cursor_t cursor; 
    unw_context_t *uc=(unw_context_t *)(a_uc); 
    unw_word_t ip, sp; 
    static char symname[10000];
	static char cxx_name[102400];

	static char outbuf[102400];
	#define uc_out(x, ...)   \
	do {\
		int len = snprintf(outbuf, sizeof(outbuf)-1, x "\n", ##__VA_ARGS__);\
		if (len > (int) sizeof(outbuf)) len = sizeof(outbuf); \
		write(fd, outbuf, len);\
	} while(0) \

	for(int i = 0; i < (int) NGREG; ++i) {
        uc_out("reg[%02d:%s]=\t0x" REGFORMAT, i, g_regs_name[i], uc->uc_mcontext.gregs[i]);
	}
    uc_out("Stack trace:");

    int ret = unw_init_local(&cursor, uc);
    do {
        if (ret) break;

        ret = unw_get_reg(&cursor, UNW_REG_IP, &ip);
        ret = unw_get_reg(&cursor, UNW_REG_SP, &sp);
        ret = unw_get_proc_name(&cursor, symname, sizeof(symname), NULL);
        size_t name_len = sizeof(cxx_name);
        int status = 0;
        __cxa_demangle(symname, cxx_name, &name_len, &status);
        char *pname = symname;
        if (status == 0) 
            pname = cxx_name;
        uc_out("%s: ip: %lx, sp: %lx", pname, (long) ip, (long) sp);
    } while (unw_step(&cursor) > 0);
    uc_out("End of stack trace.");
}

void print_stacktrace(coroutine_t *co, int fd)
{
   ucontext_t uctx;
   fcontext_t *fctx = co->fctx;
   uctx.uc_stack.stack = fctx->fc_stack.sp; 
   uctx.uc_stack.stack_size = fctx->fc_stack.size; 
   uctx.uc_mcontext.gregs[] = fctx->fc_greg[];
   print_ucontext(&co->ctx, fd); 
}

}
