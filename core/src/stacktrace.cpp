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
#include <libunwind.h>

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include "../../base/incl/addr2line.h"
#include "network_hook.h"

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
    return i-1;
}

#define BT_MAX_DEPTH  1000

int popen2(int *pipefd, char const *cmd, char * const argv[])
{
    pid_t pid = NULL;
    pipe(pipefd);
    pid = fork();
    if (pid == 0)
    { // Child
      dup2(pipefd[0], STDIN_FILENO);
      dup2(pipefd[1], STDOUT_FILENO);
      dup2(pipefd[1], STDERR_FILENO);
      execv(cmd, argv);
      exit(1);
    }
    return 0;
}

static
void 
__backtrace_symbols_fd(void **buffer, int depth, int fd, int baddr2line = 0)
{
	static char outbuf[102400];
	static char cxx_name[102400];
	static struct bt_frame bt;
    static char symname[10000];
    static char file_name[10000];

    int fds[2]={-1,-1};
    if (baddr2line)
    {
        static char cmd[1024]={0};

        snprintf(cmd, sizeof(cmd), "/proc/%d/exe", (int)(getpid()));
        static char const * args[10]={"-e", 0, "-f", "-C", "-i"};
        args[1] = cmd;
        popen2(fds, "usr/bin/addr2line", (char **)args);
    }

	if (buffer == NULL || depth <= 0)
		return ;

	for (int i = 0; i < depth; ++i) {
        //void * ip =  _Unwind_FindEnclosingFunction(buffer[i]);
		if (dladdr(buffer[i], &bt.bt_dlinfo) == 0) {
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
			    ((char *)(buffer[i]) - (char *)bt.bt_dlinfo.dli_saddr),
			    //((char *)(buffer[i]) - (char *)ip),
			    bt.bt_dlinfo.dli_fname);
		}
        if (fds[0] >=0 ) {
            static char buff[100];
            int len = 0;
            len = snprintf(buff, sizeof(buff) -1, "%p\n", buffer[i]);
            write(fds[1], buff, len);
            read(fds[0], symname, sizeof(symname)-1);
            //read(fds[0], file_name, sizeof(file_name)-1);
            uc_out("%s", symname);
        }
	}

    if (baddr2line) {
        close(fds[0]);
        close(fds[1]);
    }
	return ;
}

void print_fcontext(fcontext_t * ctx, int fd, int baddr2line=0)
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
    conet::__backtrace_symbols_fd(addrlist, addrlen, fd, baddr2line);
    uc_out("End of stack trace.");
}

void print_stacktrace(coroutine_t *co, int fd, int baddr2line=0)
{
   print_fcontext(co->fctx, fd, baddr2line); 
}

}
