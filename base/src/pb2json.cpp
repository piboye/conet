/*
 * =====================================================================================
 *
 *       Filename:  json2pb.cpp
 *
 *    Description:  
 *
 *        Version:  1.0
 *        Created:  2014年07月27日 22时55分50秒
 *       Revision:  none
 *       Compiler:  gcc
 *
 *         Author:  piboyeliu
 *   Organization:  
 *
 * =====================================================================================
 */
#include <stdlib.h>
#include <string>


#include "glog/logging.h"
#include "google/protobuf/descriptor.h"


#include "jsoncpp/json.h"
#include "jsoncpp/reader.h"
#include "jsoncpp/value.h"
#include "jsoncpp/writer.h"

#include "pb2json.h"

#include "auto_var.h"
#include "url_encode.h"
#include <stdio.h>

using namespace conet;
namespace conet
{

int pb2json(const google::protobuf::Message *msg, std::string *a_out)
{
	const google::protobuf::Descriptor *d = msg->GetDescriptor();
	if(!d)  return -1;
	size_t count = d->field_count();



#define APPEND_NAME(root, name)  \
        root.append("\""); \
        root.append(name); \
        root.append("\":"); \

    std::string &root = *a_out;

    root.append("{");

	for (size_t i = 0; i != count ; ++i)
	{
		const google::protobuf::FieldDescriptor *field = d->field(i);
		if(!field) return -2;

		const google::protobuf::Reflection *ref = msg->GetReflection();
		if(!ref)return -3;
        
        if (i > 0) {
            root.append(",");
        }
		const char *name = field->name().c_str();
		if(ref->HasField(*msg, field))
		{
			switch (field->cpp_type())
			{

            #define ENCODE_BASE(t, f, t2, fmt) \
			case google::protobuf::FieldDescriptor::CPPTYPE_##t: \
            { \
                root.append("\""); \
                root.append(name); \
                if(field->is_repeated()) { \
                    root.append("\":["); \
			        size_t count = ref->FieldSize(*msg,field); \
                    for(size_t i = 0; i != count ; ++i) { \
                        t2 val  =  ref->GetRepeated##f(*msg, field, i); \
                        char tmp[20];\
                        if (i > 0) { \
                            snprintf(tmp, sizeof(tmp), ",%" fmt, val); \
                        } else { \
                            snprintf(tmp, sizeof(tmp), "%" fmt, val); \
                        } \
                        root.append(tmp); \
                    } \
                    root.append("]"); \
			    } else {  \
                    t2 val  =  ref->Get##f(*msg, field); \
                    char tmp[30];\
                    snprintf(tmp, sizeof(tmp), "\":%" fmt, val); \
                    root.append(tmp); \
			    } \
            } \
			break \
                
                ENCODE_BASE(DOUBLE, Double, double, "lf");
                ENCODE_BASE(FLOAT, Float, float, "f");
                ENCODE_BASE(INT64, Int64, Json::Int64, "lld");
                ENCODE_BASE(UINT64, UInt64, Json::UInt64, "llu");
                ENCODE_BASE(INT32, Int32, int32_t, "d");
                ENCODE_BASE(UINT32, UInt32, uint32_t, "u");
                ENCODE_BASE(BOOL, Bool, bool, "d");

#undef ENCODE_BASE
#define APPEND_STRING(root, str) \
                root.append("\""); \
                root.append(str); \
                root.append("\""); \

		case google::protobuf::FieldDescriptor::CPPTYPE_STRING: 
        {
            root.append("\""); 
            root.append(name); 
		    if(field->is_repeated()) { 
                root.append("\":["); 
                size_t count = ref->FieldSize(*msg,field); 
                for(size_t i = 0 ; i != count ; ++i) { 
                    if (i> 0) root.append(",");
                    std::string const & val  =  ref->GetRepeatedString(*msg, field, i); 
                    if (field->type() == google::protobuf::FieldDescriptor::TYPE_BYTES) {
                        std::string val2;
                        url_encode(val, &val2);
                        APPEND_STRING(root, val2);
                    } else {
                        APPEND_STRING(root, val);
                    }
                }
                root.append("]"); 
            } else {  
                root.append("\":"); 
                if (field->type() == google::protobuf::FieldDescriptor::TYPE_BYTES) {
                    std::string val;
                    url_encode(ref->GetString(*msg, field), &val);
                    APPEND_STRING(root, val);
                } else {
                    std::string const &val = ref->GetString(*msg,field);
                    APPEND_STRING(root, val);
                }
            }
            break;
        }

		case google::protobuf::FieldDescriptor::CPPTYPE_MESSAGE:
        {
            root.append("\""); 
            root.append(name); 
            if(field->is_repeated()) { 
                root.append("\":["); 
                size_t count = ref->FieldSize(*msg,field); 
                for(size_t i = 0 ; i != count ; ++i) { 
                    if (i> 0) root.append(",");
                    AUTO_VAR(val, =,  &ref->GetRepeatedMessage(*msg, field, i)); 
                    pb2json(val, &root);
                } 
                root.append("]"); 
            } else { 
              root.append("\":"); 
              AUTO_VAR(val,  =,  &(ref->GetMessage(*msg,field)));
              pb2json(val,  &root);
            }
            break;
        }

		case google::protobuf::FieldDescriptor::CPPTYPE_ENUM:
        {
            root.append("\"");
            root.append(name);
            if(field->is_repeated()) {
                root.append("\":[");
                size_t count = ref->FieldSize(*msg,field);
                for(size_t i = 0 ; i != count ; ++i) {
                    AUTO_VAR(val, =, ref->GetRepeatedEnum(*msg,field, i));
                    char tmp[20];
                    if (i> 0)  {
                        snprintf(tmp, sizeof(tmp), ", %lu", (uint64_t) val);
                    } else {
                        snprintf(tmp, sizeof(tmp), "%lu", (uint64_t) val);
                    }
                    root.append(tmp);
                }
                root.append("]"); 
            } else {
                AUTO_VAR(val, =, ref->GetEnum(*msg,field));
                char tmp[30];
                snprintf(tmp, sizeof(tmp), "\":%lu", (uint64_t) val);
                root.append(tmp);
            }
            break;
        }
		default:
		break;
		}

		}

	}

    root.append("}");
	return 0; 
}


int pb2json(const google::protobuf::Message *msg, Json::Value * a_root)
{
	const google::protobuf::Descriptor *d = msg->GetDescriptor();
	if(!d)  return -1;
	size_t count = d->field_count();

    Json::Value &root = *a_root;

	for (size_t i = 0; i != count ; ++i)
	{
		const google::protobuf::FieldDescriptor *field = d->field(i);
		if(!field) return -2;

		const google::protobuf::Reflection *ref = msg->GetReflection();
		if(!ref)return -3;
        
		const char *name = field->name().c_str();
		if(ref->HasField(*msg, field))
		{
			switch (field->cpp_type())
			{

            #define ENCODE_BASE(t, f, t2) \
				case google::protobuf::FieldDescriptor::CPPTYPE_##t: \
		            if(field->is_repeated()) { \
                        Json::Value childs(Json::arrayValue);  \
	                    size_t count = ref->FieldSize(*msg,field); \
                        for(size_t i = 0; i != count ; ++i) { \
                            t2 val  =  ref->GetRepeated##f(*msg, field, i); \
                            childs[childs.size()]=Json::Value(val); \
                        } \
                        root[name].swap(childs); \
                    } else {  \
                        t2 val  =  ref->Get##f(*msg, field); \
                        root[name] = val; \
                    } \
                    break \
                
                ENCODE_BASE(DOUBLE, Double, double);
                ENCODE_BASE(FLOAT, Float, double);
                ENCODE_BASE(INT64, Int64, Json::Int64);
                ENCODE_BASE(UINT64, UInt64, Json::UInt64);
                ENCODE_BASE(INT32, Int32, int32_t);
                ENCODE_BASE(UINT32, UInt32, uint32_t);
                ENCODE_BASE(BOOL, Bool, bool);

#undef ENCODE_BASE

				case google::protobuf::FieldDescriptor::CPPTYPE_STRING:
                {
		            if(field->is_repeated()) 
                    { 
                        Json::Value childs(Json::arrayValue);  
	                    size_t count = ref->FieldSize(*msg,field); 
                        for(size_t i = 0 ; i != count ; ++i) { 
                            std::string const & val  =  ref->GetRepeatedString(*msg, field, i); 
                            if (field->type() == google::protobuf::FieldDescriptor::TYPE_BYTES) {
                                std::string val2;
                                url_encode(val, &val2);
                                Json::Value v2(val2);
                                childs[childs.size()].swap(v2);
                            } else {
                                Json::Value v1(val);
                                childs[childs.size()].swap(v1);
                            }
                        }
                        root[name].swap(childs); 
                    } else {  
                        if (field->type() == google::protobuf::FieldDescriptor::TYPE_BYTES) {
                            std::string val;
                            url_encode(ref->GetString(*msg, field), &val);
                            Json::Value v2(val);
                            root[name].swap(v2);
                        } else {
                            std::string const &val = ref->GetString(*msg,field);
                            Json::Value v2(val);
                            root[name].swap(v2);
                        }
                    }
					break;
                }
				case google::protobuf::FieldDescriptor::CPPTYPE_MESSAGE:
                {
                    if(field->is_repeated()) { 
                        Json::Value childs(Json::arrayValue);  
                        size_t count = ref->FieldSize(*msg,field); 
                        for(size_t i = 0 ; i != count ; ++i) { 
                            Json::Value child2(Json::objectValue);
                            AUTO_VAR(val, =,  &ref->GetRepeatedMessage(*msg, field, i)); 
                            pb2json(val, &child2);
                            childs[childs.size()].swap(child2);
                        } 
                        root[name].swap(childs); 
                     } else {  
                          Json::Value child(Json::objectValue);
                          AUTO_VAR(val,  =,  &(ref->GetMessage(*msg,field)));
                          pb2json(val,  &child);
                          root[name].swap(child);
                     }
                     break;
                 }
				case google::protobuf::FieldDescriptor::CPPTYPE_ENUM:
                {
                    if(field->is_repeated()) { 
                        Json::Value childs(Json::arrayValue);  
                        size_t count = ref->FieldSize(*msg,field); 
                        for(size_t i = 0 ; i != count ; ++i) { 
                            AUTO_VAR(val, =, ref->GetRepeatedEnum(*msg,field, i));
                            childs[childs.size()]=val->number();
                        } 
                        root[name].swap(childs); 
                     } else {  
                        AUTO_VAR(val, =, ref->GetEnum(*msg,field));
                        root[name]= val->number();
                     }
					 break;
                }
				default:
					break;
			}

		}

	}
	return 0; 
}
}

namespace conet
{


#define SET_ERROR_INFO(error_var, error_val)    \
        do { LOG(ERROR)<<error_val; if (error_var) *error_var = error_val; } while (0)

int json2pb(
    Json::Value& json_value,
    google::protobuf::Message* message,
    std::string* error,
    bool urlencoded)
{
    if (json_value.type() != Json::objectValue) {
        SET_ERROR_INFO(error, "type of json_value is not object.");
        return -10;
    }

    using namespace std;
    using namespace google::protobuf;

    const Reflection* reflection = message->GetReflection();
    const Descriptor* descriptor = message->GetDescriptor();

    vector<const FieldDescriptor*> fields;
    for (int i = 0; i < descriptor->extension_range_count(); i++) {
        const Descriptor::ExtensionRange* ext_range = descriptor->extension_range(i);
        for (int tag_number = ext_range->start; tag_number < ext_range->end; tag_number++) {
            const FieldDescriptor* field = reflection->FindKnownExtensionByNumber(tag_number);
            if (!field) continue;
            fields.push_back(field);
        }
    }
    for (int i = 0; i < descriptor->field_count(); i++) {
        fields.push_back(descriptor->field(i));
    }
    int ret = 0;

    for (size_t i = 0; i < fields.size(); i++) {
        const FieldDescriptor* field = fields[i];
        Json::Value & value = json_value[field->name()];

        if (value.isNull()) {
            if (field->is_required()) {
                SET_ERROR_INFO(error, "missed required field " + field->full_name() + ".");
                return -11;
            }
            continue;
        }
        if (field->is_repeated()) {
            if (!value.isArray()) {
                SET_ERROR_INFO(error, "invalid type for array field " + field->full_name() + ".");
                return -12;
            }
        }

#define VALUE_TYPE_CHECK(value, jsontype)                                   \
                if (!value.is##jsontype()) {                                \
                    SET_ERROR_INFO(error,                                   \
                            "invalid type for field " +                     \
                            field->full_name() + ".");                      \
                    return -13;                                           \
                }                                                           \

        switch (field->cpp_type()) {
#define CASE_FIELD_TYPE(cpptype, method, jsontype)                          \
            case FieldDescriptor::CPPTYPE_##cpptype: {                      \
                if (field->is_repeated()) {                                 \
                    for (int index = 0;                                     \
                        index < static_cast<int>(value.size());             \
                        index++) {                                          \
                        Json::Value & item = value[Json::Value::ArrayIndex(index)];  \
                        VALUE_TYPE_CHECK(item, jsontype);                   \
                        reflection->Add##method(message, field,             \
                            item.as##jsontype());                           \
                    }                                                       \
                } else {                                                    \
                    VALUE_TYPE_CHECK(value, jsontype);                      \
                    reflection->Set##method(message, field,                 \
                        value.as##jsontype());                              \
                }                                                           \
                break;                                                      \
            }                                                               \

            CASE_FIELD_TYPE(INT32,  Int32,  Int);
            CASE_FIELD_TYPE(UINT32, UInt32, UInt);
            CASE_FIELD_TYPE(FLOAT,  Float,  Double);
            CASE_FIELD_TYPE(DOUBLE, Double, Double);
            CASE_FIELD_TYPE(BOOL,   Bool,   Bool);
#undef CASE_FIELD_TYPE

#define CASE_64BIT_INT_FIELD(cpptype, method, jsontype, valuetype, func)          \
            case FieldDescriptor::CPPTYPE_##cpptype: {                      \
                if (field->is_repeated()) {                                 \
                    for (int index = 0;                                     \
                        index < static_cast<int>(value.size());             \
                        index++) {                                          \
                        Json::Value &item = value[Json::Value::ArrayIndex(index)];  \
                            valuetype number_value;                         \
                            std::string txt = item.asString();   \
                            char *p = (char *) (txt.c_str()+txt.size());  \
                            number_value = func(txt.c_str(), &p,10); \
                            reflection->Add##method(message, field,     \
                                    number_value);                          \
                    }                                                       \
                } else {                                                    \
                        valuetype number_value;                             \
                        std::string txt = value.asString();   \
                        char *p = (char *) (txt.c_str()+txt.size());  \
                        number_value = func(txt.c_str(), &p,10); \
                        reflection->Set##method(message, field,         \
                                number_value);                              \
                }                                                           \
                break;                                                      \
            }                                                               \

            CASE_64BIT_INT_FIELD(INT64,  Int64,  Int64,  int64_t, strtoll);
            CASE_64BIT_INT_FIELD(UINT64, UInt64, UInt64, uint64_t, strtoull);
#undef CASE_64BIT_INT_FIELD

            case FieldDescriptor::CPPTYPE_STRING: {
                if (field->is_repeated()) {
                    for (int index = 0;
                        index < static_cast<int>(value.size());
                        index++) {
                        Json::Value &item = value[Json::Value::ArrayIndex(index)];
                        VALUE_TYPE_CHECK(item, String);
                        string const &str = item.asString();
                        if ((field->type() == FieldDescriptor::TYPE_BYTES || urlencoded)) {
                            std::string out;
                            url_decode(str, &out);
                            reflection->AddString(message, field, out);
                        } else {
                            reflection->AddString(message, field, str);
                        }
                    }
                } else {
                    VALUE_TYPE_CHECK(value, String);
                    string const &str = value.asString();
                    if ((field->type() == FieldDescriptor::TYPE_BYTES || urlencoded)) {
                            std::string out;
                            url_decode(str, &out);
                            reflection->SetString(message, field, out);
                    } else {
                        reflection->SetString(message, field, str);
                    }
                }
                break;
            }

            case FieldDescriptor::CPPTYPE_ENUM: {
                if (field->is_repeated()) {
                    for (int index = 0;
                        index < static_cast<int>(value.size());
                        index++) {
                        Json::Value & item = value[Json::Value::ArrayIndex(index)];
                        if (!item.isInt()) {
                            SET_ERROR_INFO(error,
                                    "invalid type for field " + field->full_name() + ".");
                            return -20;
                        }
                        const EnumValueDescriptor * enum_value_descriptor =
                            field->enum_type()->FindValueByNumber(item.asInt());
                        if (!enum_value_descriptor) {
                            SET_ERROR_INFO(error,
                                    "invalid value for enum field " + field->full_name() + ".");
                            return -21;
                        }
                        reflection->AddEnum(message, field, enum_value_descriptor);
                    }
                } else {
                    if (!value.isInt()) {
                        SET_ERROR_INFO(error,
                                "invalid type for field " + field->full_name() + ".");
                        return -22;
                    }
                    const EnumValueDescriptor * enum_value_descriptor =
                        field->enum_type()->FindValueByNumber(value.asInt());
                    if (!enum_value_descriptor) {
                        SET_ERROR_INFO(error,
                                "invalid value for enum field " + field->full_name() + ".");
                        return -23;
                    }
                    reflection->SetEnum( message, field, enum_value_descriptor);
                }
                break;
            }

            case FieldDescriptor::CPPTYPE_MESSAGE: {
                if (field->is_repeated()) {
                    for (int index = 0;
                        index < static_cast<int>(value.size());
                        index++) {
                        Json::Value & item = value[Json::Value::ArrayIndex(index)];
                        if (item.isObject()) {
                            ret = json2pb(item, reflection->AddMessage(message, field), error, urlencoded); 
                            if (ret)
                            {
                                 SET_ERROR_INFO(error,
                                    "invalid type for field 1 " + field->full_name() + ".");
                                 return ret;
                                return -24;
                            }
                        } else {
                            SET_ERROR_INFO(error,
                                    "invalid type for field 2 " + field->full_name() + ".");
                            return -25;
                        }
                    }
                } else {
                    if (json2pb(value, reflection->MutableMessage(message, field), error, urlencoded))
                    {
                        SET_ERROR_INFO(error,
                                "invalid type for field 3 " + field->full_name() + ".");
                        return -26;
                    }
                }
                break;
            }
        }
    }
    return 0;
}

int json2pb(
    char const *txt, size_t len, 
    google::protobuf::Message* message,
    std::string* error,
    bool urlencoded) 
{
    Json::Value root;
    Json::Reader reader;
    if (!reader.parse(txt, txt+len,  root)) {
        SET_ERROR_INFO(error, "json string format error.");
        return -1;
    }

    return json2pb(root, message, error, urlencoded);
}

int json2pb(std::string const & val,
    google::protobuf::Message* message,
    std::string* error, bool urlencoded)
{

    return json2pb(val.c_str(), val.size(), message, error, urlencoded);
}

}

namespace conet
{

int pb2json(const google::protobuf::Descriptor *d, Json::Value * a_root)
{
	size_t count = d->field_count();

    Json::Value &root = *a_root;

	for (size_t i = 0; i != count ; ++i)
	{
		const google::protobuf::FieldDescriptor *field = d->field(i);
		if(!field) return -2;

		const char *name = field->name().c_str();
			switch (field->cpp_type())
			{

            #define ENCODE_BASE(t, f, t2) \
				case google::protobuf::FieldDescriptor::CPPTYPE_##t: \
                    if (field->is_repeated()) {            \
                        Json::Value childs(Json::arrayValue); \
                        t2 val  =  field->default_value_##f(); \
                        childs.append(val); \
                        root[name].swap(childs); \
                    } else { \
                        t2 val  =  field->default_value_##f(); \
                        root[name] = val; \
                    } \
                    break \
                
                ENCODE_BASE(DOUBLE, double, double);
                ENCODE_BASE(FLOAT, float, double);
                ENCODE_BASE(INT64, int64, Json::Int64);
                ENCODE_BASE(UINT64, uint64, Json::UInt64);
                ENCODE_BASE(INT32, int32, int32_t);
                ENCODE_BASE(UINT32, int32, uint32_t);
                ENCODE_BASE(BOOL, bool, bool);
                ENCODE_BASE(STRING, string, std::string);
            #undef ENCODE_BASE

				case google::protobuf::FieldDescriptor::CPPTYPE_MESSAGE:
                {
                    if (field->is_repeated()) {
                        Json::Value childs(Json::arrayValue); 
                        Json::Value child(Json::objectValue);
                        AUTO_VAR(val,  =,  field->message_type());
                        pb2json(val, &child);
                        childs.append(Json::Value());
                        childs[childs.size()-1].swap(child);
                        root[name].swap(childs);
                    } else {
                        Json::Value child(Json::objectValue);
                        AUTO_VAR(val,  =,  field->message_type());
                        pb2json(val, &child);
                        root[name] = child;
                    }
					break;
                }
				case google::protobuf::FieldDescriptor::CPPTYPE_ENUM:
                {
                    if (field->is_repeated()) {
                        Json::Value childs(Json::arrayValue); 
                        AUTO_VAR(val, =, field->default_value_enum());
                        childs.append(Json::Value(val->number()));
                        root[name] = childs;
                    } else {
                        AUTO_VAR(val, =, field->default_value_enum());
                        root[name] = val->number();
                    }
					break;
                }
				default:
                    assert(!"error filed type");
					break;
			}
	}
	return 0; 
}

void pb2json(const google::protobuf::Descriptor *d, std::string *out)
{
	GOOGLE_PROTOBUF_VERIFY_VERSION;
    Json::Value root(Json::objectValue);
    pb2json(d, &root);
    *out = root.toStyledString();
}

/*
void pb2json(const google::protobuf::Message *msg, std::string *out)
{
	GOOGLE_PROTOBUF_VERIFY_VERSION;
    Json::Value root(Json::objectValue);
    pb2json(msg, &root);
    *out = root.toStyledString();
}
*/

}
