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
#include "google/protobuf/descriptor.h"
#include "jsoncpp/json.h"
#include "../pb2json.h"
#include "../auto_var.h"
#include <stdio.h>
#include "../string2number.h"
#include "../plog.h"

#define SET_ERROR_INFO(error_var, error_val)    \
        do { if (error_var) *error_var = error_val; } while (0)

namespace conet
{

static int json_str_escape(std::string const &src, std::string *out)
{
    std::string &dst = *out;
    for (size_t i = 0; i < src.size(); ++i)
    {
        unsigned char ch = src[i];
        switch(ch)
        {
          case  '\"':
                dst.push_back('\\');
                dst.push_back('"');
                break;
          case  '\\':
                dst.push_back('\\');
                dst.push_back('\\');
                break;
           case    '/':
                dst.push_back('\\');
                dst.push_back('/');
                break;
           case   '\b':
                dst.push_back('\\');
                dst.push_back('b');
                break;
          case    '\f':
                dst.push_back('\\');
                dst.push_back('f');
                break;
         case     '\r':
                dst.push_back('\\');
                dst.push_back('r');
                break;
          case    '\n':
                dst.push_back('\\');
                dst.push_back('n');
                break;
          case    '\t':
                dst.push_back('\\');
                dst.push_back('t');
                break;
            default:
                dst.push_back(ch);
                    break;
        }
    }
    return 0;
}

static int json_str_unescape(std::string const &src, std::string *out)
{
    std::string &dst = *out;
    size_t len = src.size();
    for (size_t i = 0; i < len; ++i)
    {
        unsigned char ch = src[i];
        if (ch == '\\')
        {
            ++i;
            if (i >= len)
            {
               dst.push_back(ch);
               continue;
            }

            ch = src[i];

            switch(ch)
            {
                case  '"':
                    dst.push_back('"');
                    break;
                case  '\\':
                    dst.push_back('\\');
                    break;
                case  '/':
                    dst.push_back('/');
                    break;
                case  'b':
                    dst.push_back('\b');
                    break;
                case  'f':
                    dst.push_back('\f');
                    break;
                case  'r':
                    dst.push_back('\r');
                    break;
                case  'n':
                    dst.push_back('\n');
                    break;
                case  't':
                    dst.push_back('\t');
                    break;


                // \u1234 这类字符先不管

                default:
                    dst.push_back('\\');
                    dst.push_back(ch);
                    break;
            }
        }
        else
        {
            dst.push_back(ch);
        }
    }
    return 0;
}



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
        
		const char *name = field->name().c_str();
		if(field->is_repeated() || ref->HasField(*msg, field))
		{
            if (i > 0) {
                root.append(",");
            }
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
                //ENCODE_BASE(INT64, Int64, Json::Int64, "lld");
                //ENCODE_BASE(UINT64, UInt64, Json::UInt64, "llu");
                ENCODE_BASE(INT32, Int32, int32_t, "d");
                ENCODE_BASE(UINT32, UInt32, uint32_t, "u");
                ENCODE_BASE(BOOL, Bool, bool, "d");

            case google::protobuf::FieldDescriptor::CPPTYPE_INT64: 
            { 
                root.append("\""); 
                root.append(name);
                if(field->is_repeated()) { 
                    root.append("\":["); 
			        size_t count = ref->FieldSize(*msg,field); 
                    for(size_t i = 0; i != count ; ++i) { 
                        int64_t val  =  ref->GetRepeatedInt64(*msg, field, i); 
                        char tmp[30];
                        if (i > 0) { 
                            snprintf(tmp, sizeof(tmp), ",\"%ld\"" , val); 
                        } else { 
                            snprintf(tmp, sizeof(tmp), "\"%ld\"", val);
                        } 
                        root.append(tmp); 
                    } 
                    root.append("]"); 
			    } else {  
                    int64_t val  =  ref->GetInt64(*msg, field);
                    char tmp[30];
                    snprintf(tmp, sizeof(tmp), "\":\"%ld\"", val);
                    root.append(tmp); 
			    } 
            } 
            break;

            case google::protobuf::FieldDescriptor::CPPTYPE_UINT64: 
            { 
                root.append("\""); 
                root.append(name);
                if(field->is_repeated()) { 
                    root.append("\":["); 
			        size_t count = ref->FieldSize(*msg,field); 
                    for(size_t i = 0; i != count ; ++i) { 
                        uint64_t val  =  ref->GetRepeatedUInt64(*msg, field, i); 
                        char tmp[30];
                        if (i > 0) { 
                            snprintf(tmp, sizeof(tmp), ",\"%lu\"" , val); 
                        } else { 
                            snprintf(tmp, sizeof(tmp), "\"%lu\"", val);
                        } 
                        root.append(tmp); 
                    } 
                    root.append("]"); 
			    } else {  
                    uint64_t val  =  ref->GetUInt64(*msg, field); 
                    char tmp[30];
                    snprintf(tmp, sizeof(tmp), "\":\"%ld\"", val); 
                    root.append(tmp); 
			    } 
            } 
            break;


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
                        json_str_escape(val, &val2);
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
                    json_str_escape(ref->GetString(*msg, field), &val);
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
                    AUTO_VAR(val, =, ref->GetRepeatedEnum(*msg,field, i)->number());
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
                AUTO_VAR(val, =, ref->GetEnum(*msg,field)->number());
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
		if(field->is_repeated() || ref->HasField(*msg, field))
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
                //ENCODE_BASE(INT64, Int64, Json::Int64);
                //ENCODE_BASE(UINT64, UInt64, Json::UInt64);
                ENCODE_BASE(INT32, Int32, int32_t);
                ENCODE_BASE(UINT32, UInt32, uint32_t);
                ENCODE_BASE(BOOL, Bool, bool);

                case google::protobuf::FieldDescriptor::CPPTYPE_INT64: 
                    if(field->is_repeated()) 
                    { 
                        Json::Value childs(Json::arrayValue);  
                        size_t count = ref->FieldSize(*msg,field); 
                        for(size_t i = 0; i != count ; ++i) { 
                            int64_t val  =  ref->GetRepeatedInt64(*msg, field, i);
                            childs[childs.size()]=number2string(val);
                        } 
                        root[name].swap(childs); 
                    } else {  
                        int64_t val  =  ref->GetInt64(*msg, field);
                        root[name] = number2string(val); 
                    } 
                break; 

                case google::protobuf::FieldDescriptor::CPPTYPE_UINT64: 
                    if(field->is_repeated()) 
                    { 
                        Json::Value childs(Json::arrayValue);  
                        size_t count = ref->FieldSize(*msg,field); 
                        for(size_t i = 0; i != count ; ++i) { 
                            uint64_t val  =  ref->GetRepeatedUInt64(*msg, field, i);
                            childs[childs.size()]=number2string(val);
                        } 
                        root[name].swap(childs); 
                    } else {  
                        uint64_t val  =  ref->GetUInt64(*msg, field);
                        root[name] = number2string(val); 
                    } 
                break; 

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
                                json_str_escape(val, &val2);
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
                            json_str_escape(ref->GetString(*msg, field), &val);
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
                    return -__LINE__;                                       \
                }                                                           \

        switch (field->cpp_type())
        {
#define CASE_FIELD_TYPE(cpptype, method, jsontype)                          \
            case FieldDescriptor::CPPTYPE_##cpptype:                        \
            {                                                               \
                if (field->is_repeated()) {                                 \
                    for (int index = 0;                                     \
                        index < static_cast<int>(value.size());             \
                        index++) {                                          \
                        Json::Value & item = value[Json::Value::ArrayIndex(index)];  \
                        jsontype v = 0; \
                        string2number(item.asString(), &v); \
                        reflection->Add##method(message, field,v);  \
                    }                                                       \
                } else {                                                    \
                    jsontype v = 0; \
                    string2number(value.asString(), &v); \
                    reflection->Set##method(message, field,v);              \
                }                                                           \
                break;                                                      \
            }                                                               \

            CASE_FIELD_TYPE(INT32,  Int32,  int32_t);
            CASE_FIELD_TYPE(UINT32, UInt32, uint32_t);
            CASE_FIELD_TYPE(FLOAT,  Float,  double);
            CASE_FIELD_TYPE(DOUBLE, Double, double);
            CASE_FIELD_TYPE(BOOL,   Bool,   bool);
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

            case FieldDescriptor::CPPTYPE_STRING:
            {
                if (field->is_repeated()) {
                    for (int index = 0;
                        index < static_cast<int>(value.size());
                        index++) {
                        Json::Value &item = value[Json::Value::ArrayIndex(index)];
                        VALUE_TYPE_CHECK(item, String);
                        string const &str = item.asString();
                        if ((field->type() == FieldDescriptor::TYPE_BYTES || urlencoded)) {
                            std::string out;
                            //url_decode(str, &out);
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
                            //url_decode(str, &out);
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
                                "invalid type for enum field " + field->full_name() + ".");
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
                        index < static_cast<int>(value.size()); index++)
                    {
                        Json::Value & item = value[Json::Value::ArrayIndex(index)];
                        if (item.isObject()) {
                            ret = json2pb(item, reflection->AddMessage(message, field), error, urlencoded); 
                            if (ret)
                            {
                                /*
                                 SET_ERROR_INFO(error,
                                    "invalid type for field 1 " + field->full_name() + ".");
                                */
                                return ret;
                                //return -24;
                            }
                        } else {
                            SET_ERROR_INFO(error,
                                    "invalid type for field 2 " + field->full_name() + ".");
                            return -25;
                        }
                    }
                } else {
                    ret = json2pb(value, reflection->MutableMessage(message, field), error, urlencoded);
                    if (ret)
                    {
                        /*
                        SET_ERROR_INFO(error,
                                "invalid type for field 3 " + field->full_name() + ".");
                                */
                        return ret;
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
                //ENCODE_BASE(INT64, int64, Json::Int64);
                //ENCODE_BASE(UINT64, uint64, Json::UInt64);
                ENCODE_BASE(INT32, int32, int32_t);
                ENCODE_BASE(UINT32, int32, uint32_t);
                ENCODE_BASE(BOOL, bool, bool);
                ENCODE_BASE(STRING, string, std::string);
            #undef ENCODE_BASE

				case google::protobuf::FieldDescriptor::CPPTYPE_INT64: 
                    if (field->is_repeated()) { 
                        Json::Value childs(Json::arrayValue); 
                        int64_t val  =  field->default_value_int64(); 
                        childs.append(number2string(val)); 
                        root[name].swap(childs); 
                    } else { 
                        int64_t val  =  field->default_value_int64(); 
                        root[name] = number2string(val);
                    } 
                    break;
				case google::protobuf::FieldDescriptor::CPPTYPE_UINT64: 
                    if (field->is_repeated()) { 
                        Json::Value childs(Json::arrayValue); 
                        uint64_t val  =  field->default_value_uint64(); 
                        childs.append(number2string(val)); 
                        root[name].swap(childs); 
                    } else { 
                        uint64_t val  =  field->default_value_uint64(); 
                        root[name] = number2string(val);
                    } 
                    break; 

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
