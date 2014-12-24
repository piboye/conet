/*
 * pb2mysql.h
 *
 *  Created on: 2014Äê8ÔÂ25ÈÕ
 *      Author: piboyeliu
 */

#ifndef PB2SQLITE_H_
#define PB2SQLITE_H_

#include <map>
#include <string>
#include "sqlite/sqlite3.h"
#include "google/protobuf/message.h"

namespace conet 
{

    class Pb2Sqlite
    {
        public:
            Pb2Sqlite()
            {
                m_db = NULL;
                m_hold_db = 0;
            }

            ~Pb2Sqlite()
            {
                if (m_hold_db) {
                    sqlite3_close(m_db);
                }
            }

            int init(sqlite3 *db, std::string const &table);

            int init(std::string const &file_name, std::string const &table);

            int create_table(google::protobuf::Message const &proto);
            int create_table(const char * sql);

            int insert(google::protobuf::Message const & message);

            int update(google::protobuf::Message const & wheres, google::protobuf::Message const &data);

            int update(uint64_t id,  google::protobuf::Message const &updates);

            int get(google::protobuf::Message const & wheres, google::protobuf::Message *result);

            int get_all(google::protobuf::Message const & wheres, std::vector<google::protobuf::Message* > *results);

            template<typename T>
            int get_all(google::protobuf::Message const & wheres, std::vector<T*> *results)
            {
                std::vector<google::protobuf::Message *> * r2 = (std::vector<google::protobuf::Message *> *)(results);
                return get_all(wheres, r2);
            }

            int get(uint64_t id, google::protobuf::Message *result);

            int remove(uint64_t id);

            int replace(google::protobuf::Message const &updates);

            int insert_or_update(google::protobuf::Message const &updates);

            std::string m_table_name;

            sqlite3 *m_db;
            int m_hold_db;

    };
}

#endif 
