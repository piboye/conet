/*
 * =====================================================================================
 *
 *       Filename:  addr2line.h
 *
 *    Description:  
 *
 *        Version:  1.0
 *        Created:  2014年12月05日 14时17分14秒
 *       Revision:  none
 *       Compiler:  gcc
 *
 *         Author:  piboye
 *   Organization:  
 *
 * =====================================================================================
 */
#ifndef __CONET_ADDR2LINE_H__
#define __CONET_ADDR2LINE_H__
namespace conet
{

int addr2line (void *addr, char * func_name, int func_len, char *file_name,  int file_len, int *line);

}


#endif /* end of include guard */
