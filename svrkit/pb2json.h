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
#include "google/protobuf/message.h"
#include <string>
#include "jsoncpp/json.h"
#include "jsoncpp/value.h"

namespace conet
{

int  pb2json(const google::protobuf::Message *msg, Json::Value * a_root);
void pb2json(const google::protobuf::Message *msg, std::string *out);
int pb2json(const google::protobuf::Descriptor *d, Json::Value * a_root);
void pb2json(const google::protobuf::Descriptor *d, std::string *out);

int json2pb( char const *txt, size_t len, 
    google::protobuf::Message* message,
    std::string* error, bool urlencoded=false);

int json2pb(
    Json::Value& json_value,
    google::protobuf::Message* message,
    std::string* error,
    bool urlencoded=false);


int json2pb(std::string const & val,
    google::protobuf::Message* message,
    std::string* error, bool urlencoded=false);

}

#endif /* end of include guard */
