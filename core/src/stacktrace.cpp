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
#include <assert.h>
#include "coroutine_impl.h"

#include <sys/types.h>
#include <dlfcn.h>
#include <unwind.h>

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>

struct bt_frame {
	Dl_info	bt_dlinfo;
	unsigned int  line;
};



//#include "libunwind/libunwind.h"

using __cxxabiv1::__cxa_demangle;

#define REGFORMAT "%016lx"

#define uc_out(x, ...)   \
    do {\
        int len = snprintf(outbuf, sizeof(outbuf)-1, x "\n", ##__VA_ARGS__);\
        if (len > (int) sizeof(outbuf)) len = sizeof(outbuf); \
        write(fd, outbuf, len);\
    } while(0) \

/*
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
*/


namespace conet
{
/*
void print_ucontext(ucontext_t *a_uc, int fd)
{
    unw_cursor_t cursor; 
    unw_context_t *uc=(unw_context_t *)(a_uc); 
    unw_word_t ip, sp; 
    static char symname[10000];
	static char cxx_name[102400];

	static char outbuf[102400];

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
*/

static const char * g_fregs_name[8] =
{
    "",
    "R12",
    "R13",
    "R14",
    "R15",
    "RBX",
    "RBP",
    "RIP"
};


int backtrace(fcontext_t *ctx, void ** array, int num)
{
    uint64_t rbp = ctx->fc_greg[6];
    uint64_t func_addr =  0;
    int i = 0;
    array[0] = (void *) ctx->fc_greg[7]; // rip; 
    ++i;
    while (rbp > 0 && i<num)
    {
        func_addr = *(uint64_t *)(rbp+8);
        array[i] =  (void *) func_addr;
        ++i;
        rbp = *(uint64_t *)(rbp);
    }
    return i;
}

#define BT_MAX_DEPTH  1000

static
void 
__backtrace_symbols_fd(void **buffer, int depth, int fd)
{
	static char outbuf[102400];
	static char cxx_name[102400];
	static struct bt_frame bt;
	if (buffer == NULL || depth <= 0)
		return ;

	for (int i = 0; i < depth; ++i) {
        void * ip =  _Unwind_FindEnclosingFunction(buffer[i]);
		if (dladdr(ip, &bt.bt_dlinfo) == 0) {
			/* try something */
            uc_out("#%d\t%p", i, buffer[i]);
		} else {
			char const *s = (char *)bt.bt_dlinfo.dli_sname;
			if (s == NULL) {
				s = "???";
            }
            size_t name_len = sizeof(cxx_name);
            int status = 0;
            __cxa_demangle(s, cxx_name, &name_len, &status);
            char const *pname = s;
            if (status == 0) 
                pname = cxx_name;

            uc_out("#%d\t%p <%s+%ld> at %s",
                i,
			    buffer[i],
			    pname,
			    //((char *)(buffer[i]) - (char *)bt.bt_dlinfo.dli_saddr),
			    ((char *)(buffer[i]) - (char *)ip),
			    bt.bt_dlinfo.dli_fname);
		}
	}

	return ;
}

void print_fcontext(fcontext_t * ctx, int fd)
{

	static char outbuf[102400];

    uint64_t f_reg = ctx->fc_greg[0];
    uint64_t fc_mxcsr = f_reg<<32>>32;
    uint64_t fc_x87_cw = f_reg>>32;

    uc_out("reg[%02d:%s]=\t0x" REGFORMAT, 0, "fc_mxcsr", fc_mxcsr);
    uc_out("reg[%02d:%s]=\t0x" REGFORMAT, 0, "fc_x87_cw", fc_x87_cw);

	for(int i = 1; i < 8; ++i) 
    {
        uc_out("reg[%02d:%s]=\t0x" REGFORMAT, i, g_fregs_name[i], ctx->fc_greg[i]);
	}


    uc_out("Stack trace:");
    static void * addrlist[1000];
    int addrlen = conet::backtrace(ctx, addrlist, 1000);
    conet::__backtrace_symbols_fd(addrlist, addrlen, fd);
    uc_out("End of stack trace.");
}

void print_stacktrace(coroutine_t *co, int fd)
{
   print_fcontext(co->fctx, fd); 
}

}
