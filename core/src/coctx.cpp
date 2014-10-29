/*
 * =====================================================================================
 *
 *       Filename:  co_ctx.cpp
 *
 *    Description:  
 *
 *        Version:  1.0
 *        Created:  2014年10月29日 11时19分32秒
 *       Revision:  none
 *       Compiler:  gcc
 *
 *         Author:  piboye
 *   Organization:  
 *
 * =====================================================================================
 */
#include <stdlib.h>

#include "coctx.h"
#include <string.h>

#define ESP 0
#define EIP 1
// -----------
#define RSP 0
#define RIP 1
#define RBX 2
#define RDI 3
#define RSI 4

namespace conet
{
int coctx_init( coctx_t *ctx )
{
	memset( ctx,0,sizeof(*ctx));
	return 0;
}
int coctx_make( coctx_t *ctx, coctx_pfn_t pfn,const void *s,const void *s1 )
{
	char *stack = ctx->ss_sp;
	*stack = 0;

	char *sp = stack + ctx->ss_size - 1;
	sp = (char*)( ( (unsigned long)sp & -16LL ) - 8);

	int len = sizeof(coctx_param_t) + 64;
    memset( sp - len,0,len );

	ctx->routine = pfn;
	ctx->s1 = s;
	ctx->s2 = s1;

	ctx->param = (coctx_param_t*)sp;
	ctx->param->f = pfn;	
	ctx->param->f_link = 0;
	ctx->param->s1 = s;
	ctx->param->s2 = s1;

	ctx->regs[ RBX ] = stack + ctx->ss_size - 1;
	ctx->regs[ RSP ] = (char*)(ctx->param) + 8;
	ctx->regs[ RIP ] = (char*)pfn;

	ctx->regs[ RDI ] = (char*)s;
	ctx->regs[ RSI ] = (char*)s1;

	return 0;
}
}

