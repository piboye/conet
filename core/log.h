/*
 * =====================================================================================
 *
 *       Filename:  log.h
 *
 *    Description:
 *
 *        Version:  1.0
 *        Created:  2014年05月07日 17时02分45秒
 *       Revision:  none
 *       Compiler:  gcc
 *
 *         Author:  piboye
 *   Organization:
 *
 * =====================================================================================
 */
#ifndef __CO_LOG_H_INC__
#define __CO_LOG_H_INC__
#include <stdio.h>
#include "glog/logging.h"

#define LOG_SYS_CALL(func, ret) \
        LOG(ERROR)<<"syscall "<<#func <<" failed, [ret:"<<ret<<"]" \
                    "[errno:"<<errno<<"]" \
                    "[errmsg:"<<strerror(errno)<<"]" \
                     \

#endif  //__CO_LOG_H_INC__
