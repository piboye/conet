/*
 * =====================================================================================
 *
 *       Filename:  json2pb.h
 *
 *    Description:  
 *
 *        Version:  1.0
 *        Created:  2014年07月27日 22时52分47秒
 *       Revision:  none
 *       Compiler:  gcc
 *
 *         Author:  piboyeliu
 *   Organization:  
 *
 * =====================================================================================
 */

#ifndef PB2JSON_H
#define PB2JSON_H
#include "protobuf/message.h"
#include <string>

namespace conet
{
    void pb2json(const google::protobuf::Message *msg, std::string *out);
}

#endif /* end of include guard */
