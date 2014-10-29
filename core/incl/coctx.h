/*
 * =====================================================================================
 *
 *       Filename:  co_ctx.h
 *
 *    Description:  
 *
 *        Version:  1.0
 *        Created:  2014年10月29日 11时23分40秒
 *       Revision:  none
 *       Compiler:  gcc
 *
 *         Author:  piboye
 *   Organization:  
 *
 * =====================================================================================
 */

#ifndef __CO_CTX_H__
#define __CO_CTX_H__

#include <stdlib.h>


namespace conet
{
typedef int (*coctx_pfn_t)( const char *s,const char *s2 );
struct coctx_param_t
{
	coctx_pfn_t f;
	coctx_pfn_t f_link;
	const void *s1;
	const void *s2;
};
struct coctx_t
{
	void *regs[ 5 ];
	coctx_param_t *param;
	coctx_pfn_t routine;
	const void *s1;
	const void *s2;
	size_t ss_size;
	char *ss_sp;
};
int coctx_init( coctx_t *ctx );
int coctx_make( coctx_t *ctx,coctx_pfn_t pfn,const void *s,const void *s1 );
}

extern "C"
{
	extern void coctx_swap(conet::coctx_t *,conet::coctx_t* ) asm("coctx_swap");
};

#endif
