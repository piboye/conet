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

#include "pb2json.h"
#include "protobuf/descriptor.h"
#include "jsoncpp/json.h"
#include "core/incl/auto_var.h"

namespace 
{

std::string hex_encode(const std::string& input)
{
	static const char* const lut = "0123456789abcdef";
	size_t len = input.length();

	std::string output;
	output.reserve(2 * len);
	for (size_t i = 0; i < len; ++i)
	{
		const unsigned char c = input[i];
		output.push_back(lut[c >> 4]);
		output.push_back(lut[c & 15]);
	}
	return output;
}

int parse_msg(const google::protobuf::Message *msg, Json::Value * a_root);


void parse_repeated_field(
        const google::protobuf::Message *msg, 
        const google::protobuf::Reflection * ref,
        const google::protobuf::FieldDescriptor *field,
        Json::Value *a_root
        )
{
	size_t count = ref->FieldSize(*msg,field);
    Json::Value & root = *a_root;

    switch (field->cpp_type())
    {
     #define ENCODE_BASE_REP(t, f, t2) \
				case google::protobuf::FieldDescriptor::CPPTYPE_##t: \
                    {  \
                       for(size_t i = 0 ; i != count ; ++i) { \
                        t2 val  =  ref->GetRepeated##f(*msg, field, i); \
                        root.append(Json::Value(val)); \
                       } \
                    } \
                    break \

        ENCODE_BASE_REP(DOUBLE, Double, double);
        ENCODE_BASE_REP(FLOAT, Float, double);
        ENCODE_BASE_REP(INT64, Int64, Json::Int);
        ENCODE_BASE_REP(UINT64, UInt64, Json::UInt);
        ENCODE_BASE_REP(INT32, Int32, int32_t);
        ENCODE_BASE_REP(UINT32, UInt32, uint32_t);
        ENCODE_BASE_REP(BOOL, Bool, bool);

        case google::protobuf::FieldDescriptor::CPPTYPE_STRING:
        {
            std::string val;
            for(size_t i = 0 ; i != count ; ++i) { 
                val = ref->GetRepeatedString(*msg,field, i);
                root.append(Json::Value(val));
            }
            break;
        }
        case google::protobuf::FieldDescriptor::CPPTYPE_MESSAGE:
        {
            for(size_t i = 0 ; i != count ; ++i) { 
                Json::Value child;
                AUTO_VAR(val,  =,  &(ref->GetRepeatedMessage(*msg, field, i)));
                parse_msg(val,  &child);
                root.append(child);
            }
            break;
        }
        case google::protobuf::FieldDescriptor::CPPTYPE_ENUM:
        {
            for(size_t i = 0 ; i != count ; ++i) { 
                root.append(Json::Value(ref->GetRepeatedEnum(*msg,field, i)->number()));
            }
            break;
        }
        default:
            break;
    }

    return;
}

int parse_msg(const google::protobuf::Message *msg, Json::Value * a_root)
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
		if(field->is_repeated()) {
            Json::Value childs;
            parse_repeated_field(msg, ref, field, &childs);
			root[name] = childs; 
        }

		if(!field->is_repeated() && ref->HasField(*msg, field))
		{

			const google::protobuf::Message *value9;

			const google::protobuf::EnumValueDescriptor *value10;

			switch (field->cpp_type())
			{

            #define ENCODE_BASE(t, f, t2) \
				case google::protobuf::FieldDescriptor::CPPTYPE_##t: \
                    {  \
                        t2 val  =  ref->Get##f(*msg, field); \
                        root[name] = val; \
                    } \
                    break \
                
                ENCODE_BASE(DOUBLE, Double, double);
                ENCODE_BASE(FLOAT, Float, double);
                ENCODE_BASE(INT64, Int64, Json::Int);
                ENCODE_BASE(UINT64, UInt64, Json::UInt);
                ENCODE_BASE(INT32, Int32, int32_t);
                ENCODE_BASE(UINT32, UInt32, uint32_t);
                ENCODE_BASE(BOOL, Bool, bool);

				case google::protobuf::FieldDescriptor::CPPTYPE_STRING:
                {
                    std::string val;
					if (field->type() == google::protobuf::FieldDescriptor::TYPE_BYTES) {
						val = hex_encode(ref->GetString(*msg,field));
					} else {
						val = ref->GetString(*msg,field);
					}
                    root[name] = val;
					break;
                }
				case google::protobuf::FieldDescriptor::CPPTYPE_MESSAGE:
                {
                    Json::Value child;
					AUTO_VAR(val,  =,  &(ref->GetMessage(*msg,field)));

                    parse_msg(val,  &child);
                    root[name] = child;
					break;
                }
				case google::protobuf::FieldDescriptor::CPPTYPE_ENUM:
                {
					AUTO_VAR(val, =, ref->GetEnum(*msg,field));
                    root[name] = val->number();
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

void pb2json(const google::protobuf::Message *msg, std::string *out)
{
	GOOGLE_PROTOBUF_VERIFY_VERSION;
    Json::Value root;
    parse_msg(msg, &root);
    *out = root.toStyledString();
}

}

