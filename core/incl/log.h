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
 *         Author:  YOUR NAME (),
 *   Organization:
 *
 * =====================================================================================
 */
#ifndef __CO_LOG_H_INC__
#define __CO_LOG_H_INC__
#include <stdio.h>
#include "thirdparty/glog/logging.h"

#define CONET_LOG(INFO, fmt,  ...) \
    //fprintf(stderr, "[%s:%d][%s][%s]" fmt "\n", __FILE__, __LINE__, #INFO, __func__, ##__VA_ARGS__) 

#endif  //__CO_LOG_H_INC__
