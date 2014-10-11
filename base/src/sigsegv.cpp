/*
 * =====================================================================================
 *
 *       Filename:  sigsegv.cpp
 *
 *    Description:  
 *
 *
 *        Version:  1.0
 *        Created:  2014年05月27日 14时30分34秒
 *       Revision:  none
 *       Compiler:  gcc
 *
 *         Author:  piboye
 *   Organization:  
 *
 * =====================================================================================
 */
#ifndef _GNU_SOURCE
#define _GNU_SOURCE
#endif

#include <memory.h>
#include <stdlib.h>
#include <stdio.h>
#include <unistd.h>
#include <signal.h>
#include <ucontext.h>
#include <dlfcn.h>
#include <cxxabi.h>
#include <execinfo.h>
#include <libunwind.h>
#include <assert.h>

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

/*
void print_ucontext(ucontext_t *uc, int fd)
{
	static char outbuf[102400];
	static char cxx_name[102400];
	Dl_info dlinfo;
	
	#define uc_out(x, ...)   \
	do {\
		int len = snprintf(outbuf, sizeof(outbuf)-1, x "\n", ##__VA_ARGS__);\
		if (len > (int) sizeof(outbuf)) len = sizeof(outbuf); \
		write(fd, outbuf, len);\
	} while(0) \
		
	for(int i = 0; i < (int) NGREG; ++i) {
        uc_out("reg[%02d:%s]=\t0x" REGFORMAT, i, g_regs_name[i], uc->uc_mcontext.gregs[i]);
	}

    void *ip = (void*)uc->uc_mcontext.gregs[REG_RIP];
    void **bp = (void**)uc->uc_mcontext.gregs[REG_RBP];

    uc_out("Stack trace:");
	
    //static void *bt[100];
	//int sz = backtrace(bt, 100);
	//backtrace_symbols_fd(bt, sz, fd);
    //addr2line
	int f = 0;
    while(bp && ip) {
        if(!dladdr(ip, &dlinfo))
            break;
        const char *symname = dlinfo.dli_sname;
        uc_out("%s,%s", dlinfo.dli_sname, dlinfo.dli_fname);
		
        int status=0;
		
        size_t name_len = sizeof(cxx_name);
        char * name = __cxa_demangle(symname, cxx_name, &name_len, &status);

        if (status == 0)
            symname = name;

        uc_out("%2d: %p <%s+%lu> (%s)", ++f, ip, 
				 symname,
                 (unsigned long)ip - (unsigned long)dlinfo.dli_saddr,
                 dlinfo.dli_fname);
				 
        ip = bp[1];
        bp = (void**)bp[0];
    }  
    uc_out("End of stack trace.");
}
*/


void signal_handle(int signum, siginfo_t* info, void*ptr) 
{
    static const char *si_codes[3] = {"", "SEGV_MAPERR", "SEGV_ACCERR"};
	static char outbuf[10240];
    ucontext_t *ucontext = (ucontext_t*)ptr;

	int fd = 2;
	
	#define sigsegv_outp(x, ...)   \
		{\
			int len = snprintf(outbuf, sizeof(outbuf)-1, x "\n", ##__VA_ARGS__);\
			if (len > (int)sizeof(outbuf)) len = sizeof(outbuf); \
			write(fd, outbuf, len);\
		}\
			
    sigsegv_outp("Segmentation Fault!");
    sigsegv_outp("info.si_signo = %d", signum);
    sigsegv_outp("info.si_errno = %d", info->si_errno);
    sigsegv_outp("info.si_code  = %d (%s)", info->si_code, si_codes[info->si_code]);
    sigsegv_outp("info.si_addr  = %p", info->si_addr);
	//print_ucontext(ucontext, fd);
    print_ucontext(ucontext, fd);
    _exit (-1);
}


static char s_sig_stack[1024000];

static int s_catch_sig[] = {SIGILL, SIGBUS, SIGFPE, SIGABRT, SIGSEGV};

void signal_handle(int signum, siginfo_t* info, void*ptr);
static void __attribute__((constructor)) setup_sig() 
{
    static stack_t sigstack;
    sigstack.ss_sp = s_sig_stack;
    sigstack.ss_size = sizeof(s_sig_stack);
    if (sigaltstack(&sigstack, NULL) == -1) {
        perror ("sigaltstack");
    }

    struct sigaction action;
    memset(&action, 0, sizeof(action));
    action.sa_sigaction = signal_handle;
    action.sa_flags = SA_SIGINFO;

    int len = sizeof(s_catch_sig)/ sizeof(s_catch_sig[0]);
    for (int i=0; i<len; ++i) {
        if(sigaction(s_catch_sig[i], &action, NULL) < 0)
            perror("sigaction");
    }
    //ignore sigpipe
    signal(SIGPIPE, SIG_IGN);
}

