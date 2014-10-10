/*
 * pb2mysql.h
 *
 *  Created on: 2014Äê8ÔÂ25ÈÕ
 *      Author: piboyeliu
 */

#ifndef PB2SQLITE_H_
#define PB2SQLITE_H_

#include "protobuf/message.h"
#include <map>
#include <string>
#include "sqlite/sqlite3.h"

namespace conet 
{

class Pb2Sqlite
{

public:

int init(std::string const &file_name, std::string const &table);

int create_table(const char * sql);

int insert(google::protobuf::Message& message);
int update(google::protobuf::Message & wheres, google::protobuf::Message &updates);
int update(uint64_t id,  google::protobuf::Message &updates);

int get(google::protobuf::Message & wheres, google::protobuf::Message *result);
int get_all(google::protobuf::Message & wheres, std::vector<google::protobuf::Message* > *results);

int get(uint64_t id, google::protobuf::Message *result);
int remove(uint64_t id);

int replace(google::protobuf::Message const &updates);
int insert_or_update(google::protobuf::Message const &updates);

std::string m_table_name;

sqlite3 *m_db;

};


int pb2map(
    const google::protobuf::Message& message,
    std::map<std::string, std::string> *out,
    std::string* error);

}

#endif 
