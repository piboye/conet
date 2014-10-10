/*
 * Pb2Sqlite.cpp
 *
 *  Created on:
 *      Author: piboyeliu
 */

#include <map>
#include <string>

#include "gflags/gflags.h"
#include "Pb2Sqlite.h"
#include "glog/logging.h"
#include "protobuf/message.h"
#include "protobuf/descriptor.h"
namespace conet
{

    template <typename IT>
    int string2number(char const * src, IT *i)
    {
        long long n = 0;
        strtoll(src, NULL, &n);
        *i = typeof(*i) n;
        return 0;
    }

    template <typename IT>
    int string2number(std::string const &src, IT *i)
    {
        return string2number(src.c_str(), i);
    }

    template 
    int string2number<uint64_t>(char const * src, uint64_t *i)
    {
        unsigned long long n = 0;
        strtoull(src, NULL, &n);
        *i = typeof(*i) n;
        return 0;
    }

    template
    int string2number<unsigned long long>(char const * src, unsigned long long *i)
    {
        unsigned long long n = 0;
        strtoull(src, NULL, &n);
        *i = typeof(*i) n;
        return 0;
    }

    template
    int string2number<double>(char const * src, double *i)
    {
        *i = strtod(src, NULL);
        return 0;
    }

    template 
    int string2number<float>(char const * src, float *i)
    {
        *i = strtof(src, NULL);
        return 0;
    }

    template <typename IT>
    std::string number2string(IT i)
    {
        int64_t n = i;        
        std::string out;
        out.resize(20);
        size_t len = 0;
        len = snprintf(out.c_str(), out.size(), "%lld", n);
        out.resize(len);
        return out;
    }

    template 
    std::string number2string<unsigned long long>(unsigned long long i)
    {
        uint64_t n = i;        
        std::string out;
        out.resize(20);
        size_t len = 0;
        len = snprintf(out.c_str(), out.size(), "%llu", n);
        out.resize(len);
        return out;
    }

    template 
    std::string number2string<uint64_t>(uint64_t i)
    {
        uint64_t n = i;        
        std::string out;
        out.resize(20);
        size_t len = 0;
        len = snprintf(out.c_str(), out.size(), "%llu", n);
        out.resize(len);
        return out;
    }

    template 
    std::string number2string<float>(float i)
    {
        double n = i;        
        std::string out;
        out.resize(40);
        size_t len = 0;
        len = snprintf(out.c_str(), out.size(), "%f", n);
        out.resize(len);
        return out;
    }

    template 
    std::string number2string<double>(double i)
    {
        double n = i;        
        std::string out;
        out.resize(40);
        size_t len = 0;
        len = snprintf(out.c_str(), out.size(), "%f", n);
        out.resize(len);
        return out;
    }



int PB2Map(
    const google::protobuf::Message& message,
    std::map<std::string, std::string> *out,
    std::string* error)
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
                if (error)
                    error->assign("missed required field " + field->full_name() + ".");
                return -1;
            }
            continue;
        }
        if (field->is_repeated()) {
            error->assign("unsupported repeated filed");
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
                std::string val = "\"";
                val += reflection->GetString(message, field);
                val += "\"";
                out->insert(
        			std::make_pair(std::string(field->name()), val));
                break;
            }
#undef CASE_FIELD_TYPE
        default:
        	LOG(FATAL)<<"unspported [type:"<<field->cpp_type()<<"]";
        }
    }
    return 0;
}


int pb2sql_row(
    const google::protobuf::Message& message,
    std::string *field_names_list,
    std::string *value_list,
    std::string* error)
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
                if (error)
                    error->assign("missed required field " + field->full_name() + ".");
                return -1;
            }
            continue;
        }
        if (field->is_repeated()) {
            error->assign("unsupported repeated filed");
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
            std::string val = "\"";
            val += reflection->GetString(message, field);
            val += "\"";
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
        	LOG(FATAL)<<"unspported [type:"<<field->cpp_type()<<"]";
        }
    }
    return 0;
}

int sqlite2pb(
	MYSQL_RES *res,
	MYSQL_ROW row,
    google::protobuf::Message* message,
    std::string* error)
{
    using namespace std;
    using namespace google::protobuf;

    const Reflection* reflection = message->GetReflection();
    const Descriptor* descriptor = message->GetDescriptor();


    for (size_t i = 0; i < res->field_count; ++i)
    {
        std::string const &field_name = res->fields[i].name;
        const google::protobuf::FieldDescriptor* field =  descriptor->FindFieldByName(field_name);

        if (field == NULL) continue;
        if (field->is_repeated()) {
            *error = "unsupported repeated field";
            return -1;
        }

        switch (field->cpp_type()) {
#define CASE_FIELD_TYPE(cpptype, method, vtype)                          \
            case FieldDescriptor::CPPTYPE_##cpptype: {                      \
                 vtype val=vtype(); \
                 if (row[i]) { \
                    string2number(row[i], &val);  \
                    reflection->Set##method(message, field, val);           \
        		 } \
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
				 if (row[i]) {
				   std::string val = row[i];
				   reflection->SetString(message, field, val);
				 }
				break;
			}
	        default:
	        {
	        	LOG(FATAL)<<"unspported [type:"<<field->cpp_type()<<"]";
	        }
        }
    }
    return 0;
}

int Pb2Sqlite::insert(google::protobuf::Message& message)
{
	std::string field_names;
	std::string value_list;
	std::string errmsg;
	int ret = 0;
	ret = pb2sql_row(message, &field_names, &value_list, &errmsg);
	if (ret) {
		LOG(ERROR)<<"Pb2Sqlite failed [ret:"<<ret<<"][errmsg:"<<errmsg<<"]";
		return -1;
	}
	std::string query;
	query = "insert into "+ m_table_name + " (" + field_names +")"
			 + " VALUES(" + value_list + " );";

	MysqlWrapper *server = this->GetServer();
	ret = server->DoQuery(query);
	if (ret!= 0) {
		LOG(ERROR)<<"mysql failed, [sql:"<<query<<"]";
		return -1;
	}
	return server->m_rows_num == 0;
}


int Pb2Sqlite::replace(google::protobuf::Message const &updates)
{
	std::string field_names;
	std::string value_list;
	std::string errmsg;
	int ret = 0;
	ret = pb2sql_row(updates, &field_names, &value_list, &errmsg);
	if (ret) {
		LOG(ERROR)<<"Pb2Sqlite failed [ret:"<<ret<<"][errmsg:"<<errmsg<<"]";
		return -1;
	}
	std::string query;
	query = "replace "+ m_table_name + " (" + field_names +")"
			 + " VALUES(" + value_list + " );";

	MysqlWrapper *server = this->GetServer();
	ret = server->DoQuery(query);
	if (ret!= 0) {
		LOG(ERROR)<<"mysql failed, [sql:"<<query<<"]";
		return -1;
	}
	return server->m_rows_num == 0;
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

int Pb2Sqlite::update(google::protobuf::Message & wheres, google::protobuf::Message &updates)
{
	std::string field_names;
	std::string value_list;
	std::string errmsg;
	int ret = 0;
	std::map<std::string, std::string> datas;
	ret = PB2Map(updates, &datas, &errmsg);
	if (ret) {
		LOG(ERROR)<<"Pb2Sqlite failed [ret:"<<ret<<"][errmsg:"<<errmsg<<"]";
		return -1;
	}
	std::string set_values;
	get_assign_expr(datas, &set_values);


	std::map<std::string, std::string> where_datas;
	ret = PB2Map(wheres, &datas, &errmsg);
	if (ret) {
		LOG(ERROR)<<"Pb2Sqlite failed [ret:"<<ret<<"][errmsg:"<<errmsg<<"]";
		return -1;
	}
	std::string where_values;
	get_assign_expr(where_datas, &where_values);

	std::string query;
	query = "update  "+ m_table_name + " SET " + set_values + " WHERE " + where_values;

	MysqlWrapper *server = this->GetServer();
	ret = server->DoQuery(query);
	if (ret!= 0) {
		LOG(ERROR)<<"mysql failed, [sql:"<<query<<"]";
		return -1;
	}
	return server->m_rows_num == 0;
}

int Pb2Sqlite::insert_or_update(google::protobuf::Message const &updates)
{
	std::string field_names;
	std::string value_list;
	std::string errmsg;
	int ret = 0;
	std::map<std::string, std::string> datas;
	ret = PB2Map(updates, &datas, &errmsg);
	if (ret) {
		LOG(ERROR)<<"Pb2Sqlite failed [ret:"<<ret<<"][errmsg:"<<errmsg<<"]";
		return -1;
	}
	std::string set_values;
	get_assign_expr(datas, &set_values);

	ret = pb2sql_row(updates, &field_names, &value_list, &errmsg);
	if (ret) {
		LOG(ERROR)<<"Pb2Sqlite failed [ret:"<<ret<<"][errmsg:"<<errmsg<<"]";
		return -1;
	}
	std::string query;
	query = "insert into "+ m_table_name + "(" + field_names + ")"
			+ "VALUES(" + value_list + ")"
			+ " ON DUPLICATE KEY UPDATE " + set_values;

	MysqlWrapper *server = this->GetServer();
	ret = server->DoQuery(query);
	if (ret!= 0) {
		LOG(ERROR)<<"mysql failed, [sql:"<<query<<"]";
		return -1;
	}
	return server->m_rows_num == 0;
}

int Pb2Sqlite::update(uint64_t id, google::protobuf::Message &updates)
{
	std::string field_names;
	std::string value_list;
	std::string errmsg;
	int ret = 0;
	std::map<std::string, std::string> datas;
	ret = PB2Map(updates, &datas, &errmsg);
	if (ret) {
		LOG(ERROR)<<"Pb2Sqlite failed [ret:"<<ret<<"][errmsg:"<<errmsg<<"]";
		return -1;
	}
	std::string set_values;
	get_assign_expr(datas, &set_values);


	std::string query;
	query = "update "+ m_table_name + " SET " + set_values + " WHERE id=" + number2string(id);

	MysqlWrapper *server = this->GetServer();
	ret = server->DoQuery(query);
	if (ret!= 0) {
		LOG(ERROR)<<"mysql failed, [sql:"<<query<<"]";
		return -1;
	}
	return server->m_rows_num == 0;
}

int Pb2Sqlite::remove(uint64_t id)
{
	std::string field_names;
	std::string value_list;
	std::string errmsg;
	int ret = 0;

	std::string query;
	query = "DELETE from "+ m_table_name  + " WHERE id=" + number2string(id);

	MysqlWrapper *server = this->GetServer();
	ret = server->DoQuery(query);
	if (ret!= 0) {
		LOG(ERROR)<<"mysql failed, [sql:"<<query<<"]";
		return -1;
	}
	return server->m_rows_num;
}

int Pb2Sqlite::get(uint64_t id, google::protobuf::Message *out)
{
	std::string field_names;
	std::string value_list;
	std::string errmsg;
	int ret = 0;

	std::string query;
	query = "select  * from "+ m_table_name  + " WHERE id=" + number2string(id);

	MysqlWrapper *server = this->GetServer();
	ret = server->DoQuery(query);
	if (ret!= 0) {
		LOG(ERROR)<<"mysql failed, [sql:"<<query<<"]";
		return -1;
	}
	MYSQL_ROW row = mysql_fetch_row(server->m_result);
	ret = sqlite2pb(server->m_result, row, out, &errmsg);
	if (ret) {
		LOG(ERROR)<<"Pb2Sqlite failed [ret:"<<ret<<"][errmsg:"<<errmsg<<"]";
		return -1;
	}
	return ret;
}

int Pb2Sqlite::get(google::protobuf::Message & wheres, google::protobuf::Message *result)
{
	std::string field_names;
	std::string value_list;
	std::string errmsg;
	int ret = 0;

	std::map<std::string, std::string> where_datas;
	ret = PB2Map(wheres, &where_datas, &errmsg);
	if (ret) {
		LOG(ERROR)<<"Pb2Sqlite failed [ret:"<<ret<<"][errmsg:"<<errmsg<<"]";
		return -1;
	}
	std::string where_values;
	get_assign_expr(where_datas, &where_values);

	std::string query;
	query = "select  * from "+ m_table_name;
    if (!where_values.empty()) 
        query += " WHERE " + where_values + " limit 1;";

	MysqlWrapper *server = this->GetServer();
	ret = server->DoQuery(query);
	if (ret!= 0) {
		LOG(ERROR)<<"mysql failed, [sql:"<<query<<"]";
		return -1;
	}
	MYSQL_ROW row = mysql_fetch_row(server->m_result);
	if (row == NULL) {
		return 0;
	}
	ret = sqlite2pb(server->m_result, row, result, &errmsg);
	if (ret) {
		LOG(ERROR)<<"Pb2Sqlite failed [ret:"<<ret<<"][errmsg:"<<errmsg<<"]";
		return -1;
	}
	return 1;
}

int Pb2Sqlite::get_all(google::protobuf::Message & wheres, std::vector<google::protobuf::Message *> *result)
{
	std::string field_names;
	std::string value_list;
	std::string errmsg;
	int ret = 0;

	std::map<std::string, std::string> where_datas;
	ret = PB2Map(wheres, &where_datas, &errmsg);
	if (ret) {
		LOG(ERROR)<<"Pb2Sqlite failed [ret:"<<ret<<"][errmsg:"<<errmsg<<"]";
		return -1;
	}
	std::string where_values;
	get_assign_expr(where_datas, &where_values);

	std::string query;
	query = "select  * from "+ m_table_name;
    if (!where_values.empty()) 
        query += " WHERE " + where_values;

	MysqlWrapper *server = this->GetServer();
	ret = server->DoQuery(query);
	if (ret!= 0) {
		LOG(ERROR)<<"mysql failed, [sql:"<<query<<"]"<<"[ret:"<<ret<<"]";
		return -1;
	}
	int cnt = 0;
	MYSQL_ROW row = NULL;
	while ( (row = mysql_fetch_row(server->m_result)) != NULL) {
		google::protobuf::Message *msg = wheres.New();
		ret = sqlite2pb(server->m_result, row, msg, &errmsg);
		if (ret) {
				delete msg;
				LOG(ERROR)<<"Pb2Sqlite failed [ret:"<<ret<<"][errmsg:"<<errmsg<<"]";
				return -1;
		}
		result->push_back(msg);
		++cnt;
	}
	return cnt;
}

int Pb2Sqlite::init(
		std::string const &file_name,
		std::string const &table)
{
	m_table_name = table;

    int rc = 0;
    rc = sqlite3_open(file_name.c_str(), &m_db);
    if (rc) {
        LOG(ERROR)<<"init sqlite3 ["<<file_name<<"] failed,"
            "[ret:"<<rc<<"]"
            "[errmsg:"<<sqlite3_errmsg(m_db)<<"]"
        ;
        return -1;
    }
    return 0;
}

int Pb2Sqlite::create_table(const char * sql)
{

    char * errmsg = NULL;
    int rc = 0;
    rc = sqlite3_exec(m_db, sql, NULL, 0, &errmsg);
    if (rc) {
        LOG(ERROR)<<"create table failed, [sql:"<<sql<<"]"
            "[errmsg:"<<errmsg<<"]";

        sqlite3_free(errmsg);
        return -1;
    }
    return 0;
}

}
