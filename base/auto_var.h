
/*
 * =====================================================================================
 * 
 *       Filename:  auto_var.h
 * 
 *    Description:  
 * 
 *        Version:  1.0
 *        Created:  20111ʱ35ST
 *       Revision:  none
 *       Compiler:  gcc
 * 
 *         Author:  piboye
 *        Company:  
 * 
 * =====================================================================================
 */

#ifndef  AUTO_VAR_INC
#define  AUTO_VAR_INC

#define AUTO_VAR(name, op, value) typeof(value) name op (value)
#define AUTO_REF_VAR(name, op, value) typeof(value) & name op (value)

#endif   /* ----- #ifndef AUTO_VAR_INC  ----- */
