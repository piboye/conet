/*
 * =====================================================================================
 *
 *       Filename:  addr2line.cpp
 *
 *    Description:  
 *
 *        Version:  1.0
 *        Created:  2014年12月05日 11时26分46秒
 *       Revision:  none
 *       Compiler:  gcc
 *
 *         Author:  piboye
 *   Organization:  
 *
 * =====================================================================================
 */
#include <stdlib.h>
#include <stdio.h>
#include <unistd.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <fcntl.h>

#include <sys/stat.h>


#include <getopt.h>
#include <stdio.h>
#include <stdlib.h>
#include <stdarg.h>
#include <errno.h>
#include <limits.h>
#include <assert.h>
#include <cxxabi.h>
#include <execinfo.h>
#include <unwind.h>
using __cxxabiv1::__cxa_demangle;

#define PACKAGE 1
#define PACKAGE_VERSION 1
#pragma GCC visibility push(hidden) 
#include <bfd.h>
//#include <bucomm.h>
//#include <libiberty.h>
#pragma GCC visibility pop 

#include <string.h>

struct libtrace_data {
    bfd_boolean unwind_inlines;  /* Unwind inlined functions. */
    bfd_boolean with_functions;  /* Show function names.  */
    bfd_boolean do_demangle;     /* Demangle names.  */
    bfd_boolean base_names;      /* Strip directory names.  */
    asymbol **syms;              /* Symbol table.  */

    bfd *abfd;
    asection *section;
}; 

static libtrace_data m_libtrace_data={0};
/*
{
    .unwind_inlines = TRUE, 
    .with_functions = TRUE, 
    .do_demangle = TRUE, 
    .base_names = TRUE,
    .syms = NULL, 
    .abfd = NULL, 
    .section = NULL, 
};
*/


/* These variables are used to pass information between
   translate_addresses and find_address_in_section.  */
typedef struct _sym_info {
    bfd_vma pc;
    const char *filename;
    const char *functionname;
    unsigned int line;
    bfd_boolean found;
} sym_info; 

namespace conet
{
static int slurp_symtab (bfd *);
static void find_address_in_section (bfd *, asection *, void *);
static void find_offset_in_section (bfd *, asection *, sym_info *);
static int translate_addresses (bfd *abfd, asection *section, 
                  void *addr, 
                  char *buf_func, size_t buf_func_len, 
                  char *buf_file, size_t buf_file_len, int *line);

char const *program_name = "addr2line"; 


void
bfd_nonfatal(const char *string)
{
    const char *errmsg = bfd_errmsg (bfd_get_error ());
  
    if (string)
        fprintf(stderr, "%s: %s: %s\n", program_name, string, errmsg);
    else
        fprintf(stderr, "%s: %s\n", program_name, errmsg);
}

void
report(const char * format, va_list args)
{
    fprintf(stderr, "%s: ", program_name);
    vfprintf(stderr, format, args);
    putc('\n', stderr);
}

void
non_fatal(const char *format, ...)
{
    va_list args; 
        
    va_start(args, format);
    report(format, args);
    va_end(args);
}


/* Returns the size of the named file.  If the file does not
   exist, or if it is not a real file, then a suitable non-fatal
   error message is printed and zero is returned.  */

off_t
get_file_size(const char * file_name)
{
    struct stat statbuf;
    
    if (stat(file_name, &statbuf) < 0) {
        if (errno == ENOENT)
            non_fatal("'%s': No such file", file_name);
        else
            non_fatal("Warning: could not locate '%s'., errno:%d",
                  file_name, errno);
    } else if (! S_ISREG (statbuf.st_mode))
        non_fatal ("Warning: '%s' is not an ordinary file", file_name);
    else
        return statbuf.st_size;
  
    return 0;
}

/* After a FALSE return from bfd_check_format_matches with
   bfd_get_error () == bfd_error_file_ambiguously_recognized, print
   the possible matching targets.  */

void
list_matching_formats(char **p)
{
    if (!p || !*p)
        return;
        
    fprintf(stderr, "%s: Matching formats: ", program_name);
    while (*p)
        fprintf(stderr, " %s", *p++);
    fputc('\n', stderr);
}

/* Set the default BFD target based on the configured target.  Doing
   this permits the binutils to be configured for a particular target,
   and linked against a shared BFD library which was configured for a
   different target.  */

void 
set_default_bfd_target(void)
{
    /* The macro TARGET is defined by Makefile. 
       E.g.: -DTARGET='"i686-pc-linux-gnu"'.  */
    return;
    const char *target = "x86-64";
  
    if (! bfd_set_default_target(target)) {
      non_fatal("can't set BFD default target to `%s': %s",
         target, bfd_errmsg (bfd_get_error ()));
      return;
    }
    
    return; 
}


/* Read in the symbol table.  */

static int
slurp_symtab(bfd *abfd)
{
    long symcount;
    unsigned int size;
  
    if ((bfd_get_file_flags(abfd) & HAS_SYMS) == 0)
        return -1;
  
    symcount = bfd_read_minisymbols(abfd, FALSE, 
                       (void **)&m_libtrace_data.syms, &size);
    if (symcount == 0)
        symcount = bfd_read_minisymbols(abfd, TRUE /* dynamic */, 
                            (void **)&m_libtrace_data.syms, &size);
  
    if (symcount < 0) {
        bfd_nonfatal(bfd_get_filename (abfd));
        return -1;
    }
    
    return 0; 
}


/* Look for an address in a section.  This is called via
   bfd_map_over_sections.  */
   
static void
find_address_in_section(bfd *abfd, asection *section, void *data)
{
    bfd_vma vma;
    bfd_size_type size;
    sym_info *psi = (sym_info*)data; 
  
    if (psi->found)
        return;
  
    if ((bfd_get_section_flags(abfd, section) & SEC_ALLOC) == 0)
        return;
  
    vma = bfd_get_section_vma(abfd, section);
    if (psi->pc < vma)
        return;
  
    size = bfd_get_section_size(section);
    if (psi->pc >= vma + size)
        return;
  
    psi->found = bfd_find_nearest_line(abfd, section, 
                   m_libtrace_data.syms, psi->pc - vma,
                   &psi->filename, &psi->functionname, 
                   &psi->line);
}

/* Look for an offset in a section.  This is directly called.  */

static void
find_offset_in_section(bfd *abfd, asection *section, sym_info *psi)
{
    bfd_size_type size;
  
    if (psi->found)
        return;
  
    if ((bfd_get_section_flags(abfd, section) & SEC_ALLOC) == 0)
        return;
  
    size = bfd_get_section_size(section);
    if (psi->pc >= size)
        return;
  
    psi->found = bfd_find_nearest_line(abfd, section, 
                   m_libtrace_data.syms, psi->pc,
                   &psi->filename, &psi->functionname, 
                   &psi->line);
}

/* Translate xaddr into
   file_name:line_number and optionally function name.  */

static int
translate_addresses(bfd *abfd, asection *section, 
          void *xaddr, 
          char *buf_func, size_t buf_func_len, 
          char *buf_file, size_t buf_file_len, int *line_no)  
{
    #define ADDR_BUF_LEN ((CHAR_BIT/4)*(sizeof(void*))+1)
    char addr[ADDR_BUF_LEN+1] = {0};
    sym_info si = {0}; 

    sprintf(addr, "%p", xaddr);
    si.pc = bfd_scan_vma (addr, NULL, 16);

	*line_no = 0;
    si.found = FALSE;
    if (section)
        find_offset_in_section(abfd, section, &si);
    else
        bfd_map_over_sections(abfd, find_address_in_section, &si);

    if (! si.found) {
        if (buf_func != NULL)
            snprintf(buf_func, buf_func_len, "%s ??", 
                    m_libtrace_data.with_functions ? "??" : "");
    } else {
          do {
            if (m_libtrace_data.with_functions) {
                const char *name;
                static char cxx_name[1024];
                size_t name_len = sizeof(cxx_name);

                name = si.functionname;
                if (name == NULL || *name == '\0')
                    name = "??";
                else if (m_libtrace_data.do_demangle) {
                    int status = 0;
                    __cxa_demangle(name, cxx_name, &name_len, &status);
                    name = cxx_name;
                }

                if (buf_func != NULL)
                    snprintf(buf_func, buf_func_len, "%s", name);
            }

            if (m_libtrace_data.base_names && si.filename != NULL) {
                //char *h = strrchr(si.filename, '/');
                char const *h = si.filename;
                if (h != NULL)
                    si.filename = h + 1;
            }

            if (buf_file != NULL) {
                snprintf(buf_file, buf_file_len, "%s", 
                        si.filename ? si.filename : "??");
				*line_no = si.line;
			}
            if (!m_libtrace_data.unwind_inlines)
                si.found = FALSE;
            else
                si.found = bfd_find_inliner_info(abfd, 
                            &si.filename, 
                            &si.functionname, 
                            &si.line);
          } while (si.found);

    }

    return si.found;
}

/* --------------------------------------------------------------- */

int 
libtrace_init(
          const char *file_name, 
          const char *section_name,
          const char *target)
{
    char **matching = NULL;
  
    bfd_init();
    set_default_bfd_target();
  
    if (get_file_size(file_name) < 1)
      return -1;
  
    m_libtrace_data.abfd = bfd_openr(file_name, target);
    if (m_libtrace_data.abfd == NULL) {
        bfd_nonfatal (file_name);
        return -1; 
    }
  
    if (bfd_check_format(m_libtrace_data.abfd, bfd_archive)) {
        non_fatal ("%s: cannot get addresses from archive", file_name);
        return -1; 
    }
  
    if (!bfd_check_format_matches(m_libtrace_data.abfd, bfd_object, &matching)) {
        bfd_nonfatal(bfd_get_filename(m_libtrace_data.abfd));
        if (bfd_get_error() == bfd_error_file_ambiguously_recognized) {
            list_matching_formats(matching);
            free (matching);
        }
        return -1;
    }
  
    if (section_name != NULL) {
        m_libtrace_data.section = bfd_get_section_by_name(m_libtrace_data.abfd, 
                                          section_name);
        if (m_libtrace_data.section == NULL)
            non_fatal("%s: cannot find section %s", file_name, section_name);
    } else
        m_libtrace_data.section = NULL;
  
    if (0 != slurp_symtab(m_libtrace_data.abfd))
        return -1;
  
    return 0; 
}

void libtrace_close(void)
{
    if (m_libtrace_data.syms != NULL) {
        free (m_libtrace_data.syms);
        m_libtrace_data.syms = NULL;
    }
  
    bfd_close(m_libtrace_data.abfd);
  
    return ;
}

int 
libtrace_resolve(
          void *addr, 
          char *buf_func, size_t buf_func_len, 
          char *buf_file, size_t buf_file_len,  
          int *line
          )
{
    int ret = FALSE; 
    ret = translate_addresses(m_libtrace_data.abfd, 
                  m_libtrace_data.section, addr, 
                  buf_func, buf_func_len,
                  buf_file, buf_file_len, line);
    assert(0 == ret); 
    return 0; 
}


int get_exec_name(char * name, int max_len)
{
	static char cmd[1024];
	return snprintf(name, max_len, "/proc/%d/exe", (int)getpid());
    /*
	int fd = open(cmd, O_RDONLY);
	if (fd)
	{
		return -1;
	}
	int len = read(fd, name, max_len);
	close(fd);
    */
	//return len;
}

int addr2line (void *addr, char * func_name, int func_len, char *file_name,  int file_len, int *line)
{
    
	int ret = 0;
	
	static int init =0;
	if (init == 0) 
	{
	    static char exe_name[1024];
        get_exec_name(exe_name, sizeof(exe_name));
		ret = libtrace_init(exe_name, NULL, NULL);
		if (ret) {
			init = -1;
			return -1;
		}
		init = 1;
		atexit(libtrace_close);
	}
	
	if (init > 0)
	{
        libtrace_resolve(addr, func_name, func_len, file_name, file_len, line);
		return 0;
    }
    return -2; 
}

}
