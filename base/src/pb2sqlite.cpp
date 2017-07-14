/*
 * Pb2Sqlite.cpp
 *
 *  Created on:
 *      Author: piboyeliu
 */

#include <map>
#include <string>
#include <stdlib.h>
#include <stdio.h>
#include <inttypes.h>

#include "gflags/gflags.h"
#include "../plog.h"
#include "google/protobuf/message.h"
#include "google/protobuf/descriptor.h"
#include "pb2sqlite.h"
#include "string2number.h"


namespace conet
{

int PB2Map(
    const google::protobuf::Message& message,
    std::map<std::string, std::string> *out)
{
    using namespace std;
    using namespace google::protobuf;

    const Reflection* reflection = message.GetReflection();
    const Descriptor* descriptor = message.GetDescriptor();

    vector<const FieldDescriptor*> fields;
    for (size_t i = 0, num = descriptor->extension_range_count(); i<num; i++)
    {
        const Descriptor::ExtensionRange * ext_range = descriptor->extension_range(i);
        for (int tag_number = ext_range->start; tag_number < ext_range->end; tag_number++) {
            const FieldDescriptor* field = reflection->FindKnownExtensionByNumber(tag_number);
            if (!field) continue;
            fields.push_back(field);
        }
    }
    for (int i = 0; i < descriptor->field_count(); i++) {
        fields.push_back(descriptor->field(i));
    }

    for (size_t i = 0; i < fields.size(); i++) {
        const FieldDescriptor* field = fields[i];
        if (!field->is_repeated() && !reflection->HasField(message, field)) {
            if (field->is_required()) {
                PLOG_ERROR("missed required field ", field->full_name(), ".");
                return -1;
            }
            continue;
        }
        if (field->is_repeated()) {
            PLOG_ERROR("unsupported repeated field, [field_name:", field->full_name(), "]");
            return -1;
        }

        switch (field->cpp_type()) {
#define CASE_FIELD_TYPE(cpptype, method, vtype)                          \
            case FieldDescriptor::CPPTYPE_##cpptype: {                      \
                vtype val = (vtype )(reflection->Get##method(message, field)); \
                out->insert( \
        			std::make_pair(std::string(field->name()), number2string(val))); \
                break;                                                      \
            }                                                               \

            CASE_FIELD_TYPE(INT32,  Int32, int32_t);
            CASE_FIELD_TYPE(UINT32, UInt32, uint32_t);
            CASE_FIELD_TYPE(FLOAT,  Float, float);
            CASE_FIELD_TYPE(BOOL,   Bool,  int32_t);
            CASE_FIELD_TYPE(INT64,  Int64, int64_t);
            CASE_FIELD_TYPE(UINT64, UInt64, uint64_t);
            CASE_FIELD_TYPE(DOUBLE, Double, double);

            case FieldDescriptor::CPPTYPE_STRING: {
                char * p = sqlite3_mprintf("%Q", reflection->GetString(message, field).c_str());
                std::string val(p);
                sqlite3_free(p);
                out->insert(
        			std::make_pair(std::string(field->name()), val));
                break;
            }
#undef CASE_FIELD_TYPE
        default:
        	PLOG_FATAL("unspported [type:", field->cpp_type(), "] [field_name:", field->full_name(), "]");
        }
    }
    return 0;
}


int pb2sql_row(
    const google::protobuf::Message& message,
    std::string *field_names_list,
    std::string *value_list)
{
    using namespace std;
    using namespace google::protobuf;

    const Reflection* reflection = message.GetReflection();
    const Descriptor* descriptor = message.GetDescriptor();

    vector<const FieldDescriptor*> fields;
    for (size_t i = 0, num = descriptor->extension_range_count(); i<num; i++)
    {
        const Descriptor::ExtensionRange * ext_range = descriptor->extension_range(i);
        for (int tag_number = ext_range->start; tag_number < ext_range->end; tag_number++) {
            const FieldDescriptor* field = reflection->FindKnownExtensionByNumber(tag_number);
            if (!field) continue;
            fields.push_back(field);
        }
    }
    for (int i = 0; i < descriptor->field_count(); i++) {
        fields.push_back(descriptor->field(i));
    }

    std::string & sql_fields = *field_names_list;
    std::string & sql_values = *value_list;
    int count = 0;
    for (size_t i = 0; i < fields.size(); i++) {
        const FieldDescriptor* field = fields[i];
        if (!field->is_repeated() && !reflection->HasField(message, field)) {
            if (field->is_required()) {
                PLOG_ERROR("missed required field ", field->full_name(),  ".");
                return -1;
            }
            continue;
        }
        if (field->is_repeated()) {
            PLOG_ERROR("unsupported repeated field, [field_name:", field->full_name(), "]");
            return -1;
        }

        switch (field->cpp_type()) {
#define CASE_FIELD_TYPE(cpptype, method, vtype)                          \
            case FieldDescriptor::CPPTYPE_##cpptype: {                      \
                vtype val = (vtype )(reflection->Get##method(message, field)); \
                if (count != 0) { \
                  sql_fields += ","; \
                  sql_values += ","; \
                } \
                sql_fields += field->name(); \
                sql_values += number2string(val); \
                ++count; \
                break;    \
            }   \

            CASE_FIELD_TYPE(INT32,  Int32, int32_t);
            CASE_FIELD_TYPE(UINT32, UInt32, uint32_t);
            CASE_FIELD_TYPE(FLOAT,  Float, float);
            CASE_FIELD_TYPE(BOOL,   Bool,  int32_t);
            CASE_FIELD_TYPE(INT64,  Int64, int64_t);
            CASE_FIELD_TYPE(UINT64, UInt64, uint64_t);
            CASE_FIELD_TYPE(DOUBLE, Double, double);

        case FieldDescriptor::CPPTYPE_STRING: {
            char * p = sqlite3_mprintf("%Q", reflection->GetString(message, field).c_str());
            std::string val(p);
            sqlite3_free(p);
            if (count != 0) {
            	sql_fields += ",";
            	sql_values += ",";
            }
            sql_fields += field->name();
            sql_values += val;
            ++count;
            break;
        }
#undef CASE_FIELD_TYPE
        default:
        	PLOG_FATAL("unspported [type:", field->cpp_type(), "] [field_name:", field->full_name(), "]");
        }
    }
    return 0;
}

class SqliteData
{
public:
    Pb2Sqlite * server;
    google::protobuf::Message const * msg_proto; 
    std::vector<google::protobuf::Message *> result;

    ~SqliteData()
    {
        if (!result.empty()) {
              for (size_t i = 0, len = result.size(); i<len; ++i)
              {
                  delete result[i];
              }
              result.clear();
        }
    }
};

int sqlite2pb(
    void * a_self, 
    int count,
    char ** values,
    char ** names)
{
    using namespace std;
    using namespace google::protobuf;

    SqliteData * self = (SqliteData *)(a_self);

    google::protobuf::Message *msg = self->msg_proto->New();

    self->result.push_back(msg);

    const Reflection* reflection = msg->GetReflection();
    const Descriptor* descriptor = msg->GetDescriptor();


    for (int i = 0; i < count; ++i)
    {
        if (NULL == names[i]) continue;
        if (NULL == values[i]) continue;

        std::string const &field_name = names[i];
        const google::protobuf::FieldDescriptor* field =  descriptor->FindFieldByName(field_name);

        if (field == NULL)  {
            PLOG_ERROR("sqlite result with unkown [field_name:", field_name, "], pb nomatch");
            continue;
        }
        if (field->is_repeated()) {
            PLOG_ERROR("unsupported repeated [field_name:", field_name, "]");
            return -1;
        }

        switch (field->cpp_type()) {
#define CASE_FIELD_TYPE(cpptype, method, vtype)                          \
            case FieldDescriptor::CPPTYPE_##cpptype: {                      \
                 vtype val=vtype(); \
                    string2number(values[i], &val);  \
                    reflection->Set##method(msg, field, val);           \
                break;                                                      \
            }                                                               \

            CASE_FIELD_TYPE(INT32,  Int32,  int32_t);
            CASE_FIELD_TYPE(UINT32, UInt32, uint32_t);
            CASE_FIELD_TYPE(FLOAT,  Float,  float);
            CASE_FIELD_TYPE(BOOL,   Bool,   int32_t);
            CASE_FIELD_TYPE(INT64,  Int64, int64_t);
            CASE_FIELD_TYPE(UINT64, UInt64, uint64_t);
            CASE_FIELD_TYPE(DOUBLE, Double, double);

			case FieldDescriptor::CPPTYPE_STRING: {
				   std::string val = values[i];
				   reflection->SetString(msg, field, val);
				break;
			}
	        default:
	        {
	        	PLOG_FATAL("unspported [type:", field->cpp_type(), "] [field_name:", field_name, "]");
	        }
        }
    }
    return 0;
}

int Pb2Sqlite::insert(google::protobuf::Message const & message)
{
	std::string field_names;
	std::string value_list;
	char * errmsg = NULL;
	int ret = 0;
	ret = pb2sql_row(message, &field_names, &value_list);
	if (ret) {
		PLOG_ERROR("Pb2Sqlite failed [ret:", ret, "]");
		return -1;
	}
	std::string query;
	query = "insert into "+ m_table_name + " (" + field_names +")"
			 + " VALUES(" + value_list + " );";

    ret = sqlite3_exec(m_db, query.c_str(), NULL, NULL, &errmsg);
	if (ret!= 0) {
		PLOG_ERROR("Pb2Sqlite.insert failed, ", (ret, errmsg, query));
        sqlite3_free(errmsg);
		return -1;
	}

	return 0;
}


int Pb2Sqlite::replace(google::protobuf::Message const &msg)
{
	std::string field_names;
	std::string value_list;
	char * errmsg = NULL;
	int ret = 0;
	ret = pb2sql_row(msg, &field_names, &value_list);
	if (ret) {
		PLOG_ERROR("Pb2Sqlite.replace failed [ret:", ret, "]");
		return -1;
	}
	std::string query;
	query = "replace "+ m_table_name + " (" + field_names +")"
			 + " VALUES(" + value_list + " );";

    ret = sqlite3_exec(m_db, query.c_str(), NULL, NULL, &errmsg);
	if (ret!= 0) {
		PLOG_ERROR("Pb2Sqlite.replace failed, ", (ret, errmsg, query));
        sqlite3_free(errmsg);
		return -1;
	}

	return 0;
}

void get_assign_expr(
	std::map<std::string, std::string> const &datas,
	std::string *out)
{
	typeof(datas.begin()) it = datas.begin(), iend = datas.end();
	int i = 0;
	for (; it!=iend; ++it, ++i) {
		if (i > 0) *out += ",";
		*out += it->first + "=" + it->second;
	}
}

int Pb2Sqlite::update(google::protobuf::Message const & wheres, google::protobuf::Message const & msg)
{
	std::string field_names;
	std::string value_list;
	char * errmsg = NULL;
	int ret = 0;
	std::map<std::string, std::string> datas;
	ret = PB2Map(msg, &datas);
	if (ret) {
		PLOG_ERROR("Pb2Sqlite failed [ret:", ret, "]");
		return -1;
	}
	std::string set_values;
	get_assign_expr(datas, &set_values);


	std::map<std::string, std::string> where_datas;
	ret = PB2Map(wheres, &datas);
	if (ret) {
		PLOG_ERROR("Pb2Sqlite failed [ret:", ret, "]");
		return -1;
	}
	std::string where_values;
	get_assign_expr(where_datas, &where_values);

	std::string query;
	query = "update  "+ m_table_name + " SET " + set_values + " WHERE " + where_values;

    ret = sqlite3_exec(m_db, query.c_str(), NULL, NULL, &errmsg);
	if (ret!= 0) {
		PLOG_ERROR("Pb2Sqlite.update failed, ", (ret, errmsg, query));
        sqlite3_free(errmsg);
		return -1;
	}

    return 0;
}

int Pb2Sqlite::insert_or_update(google::protobuf::Message const &msg)
{
	std::string field_names;
	std::string value_list;
    char *errmsg = NULL;
	int ret = 0;
	std::map<std::string, std::string> datas;
	ret = PB2Map(msg, &datas);
	if (ret) {
		PLOG_ERROR("Pb2Sqlite failed [ret:", ret, "]");
		return -1;
	}
	std::string set_values;
	get_assign_expr(datas, &set_values);

	ret = pb2sql_row(msg, &field_names, &value_list);
	if (ret) {
		PLOG_ERROR("Pb2Sqlite failed [ret:", ret, "]");
		return -1;
	}
	std::string query;
	query = "insert into "+ m_table_name + "(" + field_names + ")"
			+ "VALUES(" + value_list + ")"
			+ " ON DUPLICATE KEY UPDATE " + set_values;

    ret = sqlite3_exec(m_db, query.c_str(), NULL, NULL, &errmsg);
	if (ret!= 0) {
		PLOG_ERROR("Pb2Sqlite.insert_or_update failed, ", (ret, errmsg, query));
        sqlite3_free(errmsg);
		return -1;
	}

	return 0;
}

int Pb2Sqlite::update(uint64_t id, google::protobuf::Message const &msg)
{
	std::string field_names;
	std::string value_list;
	char * errmsg = NULL;
	int ret = 0;
	std::map<std::string, std::string> datas;
	ret = PB2Map(msg, &datas);
	if (ret) {
		PLOG_ERROR("Pb2Sqlite.pb2map failed [ret:", ret, "]");
		return -1;
	}
	std::string set_values;
	get_assign_expr(datas, &set_values);


	std::string query;
	query = "update "+ m_table_name + " SET " + set_values + " WHERE id=" + number2string(id);

    ret = sqlite3_exec(m_db, query.c_str(), NULL, NULL, &errmsg);
	if (ret!= 0) {
		PLOG_ERROR("Pb2Sqlite.update failed, ", (ret, errmsg, query));
        sqlite3_free(errmsg);
		return -1;
	}

	return 0;
}

int Pb2Sqlite::remove(uint64_t id)
{
	std::string field_names;
	std::string value_list;
	char * errmsg = NULL;
	int ret = 0;

	std::string query;
	query = "DELETE from "+ m_table_name  + " WHERE id=" + number2string(id);

    ret = sqlite3_exec(m_db, query.c_str(), NULL, NULL, &errmsg);
	if (ret!= 0) {
		PLOG_ERROR("Pb2Sqlite.update failed, ", (ret, errmsg, query));
        sqlite3_free(errmsg);
		return -1;
	}
    return 0;
}

int Pb2Sqlite::get(uint64_t id, google::protobuf::Message *out)
{
	std::string field_names;
	std::string value_list;
	char * errmsg = NULL;
	int ret = 0;

	std::string query;
	query = "select  * from "+ m_table_name  + " WHERE id=" + number2string(id);

    SqliteData data;
    data.msg_proto = out;

    ret = sqlite3_exec(m_db, query.c_str(), &sqlite2pb, &data, &errmsg);

	if (ret) {
		PLOG_ERROR("pb2sqlite.get failed, ", (ret, errmsg));
        sqlite3_free(errmsg);
		return -1;
	}

    if (!data.result.empty()) {
        out->CopyFrom(*data.result[0]);
    }

	return ret;
}

int Pb2Sqlite::get(google::protobuf::Message const & wheres, google::protobuf::Message *result)
{
	std::string field_names;
	std::string value_list;
	char * errmsg = NULL;
	int ret = 0;

	std::map<std::string, std::string> where_datas;
	ret = PB2Map(wheres, &where_datas);
	if (ret) {
		PLOG_ERROR("Pb2Sqlite.pb2map failed [ret=", ret, "]");
		return -1;
	}
	std::string where_values;
	get_assign_expr(where_datas, &where_values);

	std::string query;
	query = "select  * from "+ m_table_name;
    if (!where_values.empty()) 
        query += " WHERE " + where_values + " limit 1;";

    SqliteData data;
    data.msg_proto = result;

    ret = sqlite3_exec(m_db, query.c_str(), sqlite2pb, &data, &errmsg);

	if (ret) {
		PLOG_ERROR("pb2sqlite.get failed, ", (ret, errmsg));
        sqlite3_free(errmsg);
		return -1;
	}

    if (!data.result.empty()) {
        result->CopyFrom(*data.result[0]);
    }
	return 1;
}

int Pb2Sqlite::get_all(google::protobuf::Message const & wheres, std::vector<google::protobuf::Message *> *result)
{
	std::string field_names;
	std::string value_list;
	char * errmsg = NULL;
	int ret = 0;

	std::map<std::string, std::string> where_datas;
	ret = PB2Map(wheres, &where_datas);
	if (ret) {
		PLOG_ERROR("Pb2Sqlite.pb2map failed [ret=", ret, "]");
		return -1;
	}
	std::string where_values;
	get_assign_expr(where_datas, &where_values);

	std::string query;
	query = "select  * from "+ m_table_name;
    if (!where_values.empty()) 
        query += " WHERE " + where_values;

    SqliteData data;
    data.msg_proto = &wheres;

    ret = sqlite3_exec(m_db, query.c_str(), sqlite2pb, &data, &errmsg);

	if (ret) {
		PLOG_ERROR("pb2sqlite.get failed, ", (ret, errmsg));
        sqlite3_free(errmsg);
		return -1;
	}

    result->swap(data.result);

	return result->size();
}

int Pb2Sqlite::init(
		std::string const &file_name,
		std::string const &table)
{
	m_table_name = table;

    int rc = 0;
    rc = sqlite3_open(file_name.c_str(), &m_db);
    if (rc) {
        PLOG_ERROR("init sqlite3 [", file_name, "] failed,"
            "[ret:", rc, "]"
            "[errmsg:", sqlite3_errmsg(m_db), "]");
        return -1;
    }
    m_hold_db = 1;
    return 0;
}

int Pb2Sqlite::init(sqlite3 *db, std::string const &table)
{
    m_db = db;
    m_table_name = table;
    m_hold_db = 0;
    return 0;
}

int Pb2Sqlite::create_table(const char * sql)
{

    char * errmsg = NULL;
    int ret = 0;
    ret = sqlite3_exec(m_db, sql, NULL, 0, &errmsg);
    if (ret) {
        PLOG_ERROR("create table failed, ", (ret, errmsg, sql));

        sqlite3_free(errmsg);
        return -1;
    }
    return 0;
}

namespace 
{
struct FieldType
{
    std::string name;
    std::string type;
};
}

static
int Descriptor2SqlType(
    const google::protobuf::Descriptor* descriptor,
    std::vector<FieldType> *out)
{
    using namespace std;
    using namespace google::protobuf;

    vector<const FieldDescriptor*> fields;
    for (int i = 0; i < descriptor->field_count(); i++) {
        fields.push_back(descriptor->field(i));
    }

    for (size_t i = 0; i < fields.size(); i++) {
        const FieldDescriptor* field = fields[i];
        if (field->is_repeated()) {
            PLOG_ERROR("unsupported repeated field, [field_name:", field->full_name(), "]");
            return -1;
        }

        switch (field->cpp_type()) {

#undef CASE_FIELD_TYPE
#define CASE_FIELD_TYPE(cpptype, vtype)                          \
            case FieldDescriptor::CPPTYPE_##cpptype: {           \
                FieldType ft; \
                ft.name = field->name(); \
                ft.type= vtype; \
                out->push_back(ft) ; \
                break;                                \
            }                                         \

            CASE_FIELD_TYPE(INT32,  "int");
            CASE_FIELD_TYPE(UINT32, "int");
            CASE_FIELD_TYPE(FLOAT,  "REAL");
            CASE_FIELD_TYPE(BOOL,   "int");
            CASE_FIELD_TYPE(INT64,  "int");
            CASE_FIELD_TYPE(UINT64, "int");
            CASE_FIELD_TYPE(DOUBLE, "REAL");
            CASE_FIELD_TYPE(STRING, "BLOB");

#undef CASE_FIELD_TYPE
        default:
        	PLOG_FATAL("unspported [type:", field->cpp_type(), "] [field_name:", field->full_name(), "]");

        }
    }
    return 0;
}

int Pb2Sqlite::create_table(google::protobuf::Message const &proto)
{
    std::vector<FieldType> field_types;

    int ret  = 0;
    ret = Descriptor2SqlType( proto.GetDescriptor(), &field_types); 
    if (ret) {
        return -1;
    }

    std::string sql = " create table if not exists ";
    sql += "\"" + m_table_name + "\" (";

    for(size_t i=0, len = field_types.size(); i<len; ++i)
    {
        if (i >0) sql += ",";
        sql+=  "\"" + field_types[i].name + "\" " + field_types[i].type;
    }

    sql += " );";
    return create_table(sql.c_str());
}

}
