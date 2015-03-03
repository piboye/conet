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
#include "../../base/addr2line.h"
#include "network_hook.h"

struct bt_frame {
	Dl_info	bt_dlinfo;
	unsigned int  line;
};


using __cxxabiv1::__cxa_demangle;

#define REGFORMAT "%016lx"

#define uc_out(x, ...)   \
    do {\
        int len = snprintf(outbuf, sizeof(outbuf)-1, x "\n", ##__VA_ARGS__);\
        if (len > (int) sizeof(outbuf)) len = sizeof(outbuf); \
        write(fd, outbuf, len);\
    } while(0) \


namespace conet
{
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
    pid_t pid = 0;
    int fds1[2]={-1, -1};
    int fds2[2]={-1, -1};
    pipe(fds1);
    pipe(fds2);
    pid = fork();
    if (pid == 0)
    { // Child
      dup2(fds1[0], STDIN_FILENO);
      dup2(fds2[1], STDOUT_FILENO);
      dup2(fds2[1], STDERR_FILENO);
      close(fds1[0]);
      close(fds1[1]);
      close(fds2[0]);
      close(fds2[1]);
      execvp(cmd, argv);
      exit(1);
    }

    close(fds1[0]);
    close(fds2[1]);

    pipefd[0]=fds2[0];
    pipefd[1]=fds1[1];
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

    int fds[2]={-1,-1};
    if (baddr2line)
    {
        char const *cmd = "/usr/bin/addr2line";
        static char exe[100]={0};
        snprintf(exe, sizeof(exe)-1, "/proc/%d/exe", (int)(getpid()));
        char const * args[]={cmd, "-e", exe, "-f", "-C", "-i", NULL};
        popen2(fds, cmd, (char **)args);
    }

	if (buffer == NULL || depth <= 0)
		return ;

	for (int i = 0; i < depth; ++i) {
        if (baddr2line && fds[0] >=0 ) {
            static char buff[100];
            int len = 0;
            len = snprintf(buff, sizeof(buff) -1, "%p\n", buffer[i]);
            write(fds[1], buff, len);
            read(fds[0], symname, sizeof(symname)-1);
            char *file_name = strchr(symname, '\n');
            *file_name = 0;
            file_name +=1;
            char *p = strchr(file_name, '\n');
            if (p) {
                *p = 0;
            }
            uc_out("#%d\t%p\t%s in %s", i, buffer[i], symname, file_name);
        } else {
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
