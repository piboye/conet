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

void json2pb(Json::Value *root, google::protobuf::Message* msg);

void json2field(Json::Value *root, 
        google::protobuf::Message *msg, 
        const google::protobuf::FieldDescriptor *field)
{
	const Reflection *ref = msg->GetReflection();
	const bool repeated = field->is_repeated();

	switch (field->cpp_type())
	{

#define _SET_OR_ADD(sfunc, afunc, value)			\
		do {						\
			if (repeated)				\
				ref->afunc(&msg, field, value);	\
			else					\
				ref->sfunc(&msg, field, value);	\
		} while (0)

#define _CONVERT(type, ctype, fmt, sfunc, afunc) 		\
		case FieldDescriptor::type: {			\
			ctype value;				\
			int r = json_unpack_ex(jf, &error, JSON_STRICT, fmt, &value); \
			if (r) throw j2pb_error(field, std::string("Failed to unpack: ") + error.text); \
			_SET_OR_ADD(sfunc, afunc, value);	\
			break;					\
		}


        case google::protobuf::FieldDescriptor::CPPTYPE_DOUBLE:
            {
                double value = root[

            }
            break;

		_CONVERT(CPPTYPE_DOUBLE, double, "f", SetDouble, AddDouble);
		_CONVERT(CPPTYPE_FLOAT, double, "f", SetFloat, AddFloat);
		_CONVERT(CPPTYPE_INT64, json_int_t, "I", SetInt64, AddInt64);
		_CONVERT(CPPTYPE_UINT64, json_int_t, "I", SetUInt64, AddUInt64);
		_CONVERT(CPPTYPE_INT32, json_int_t, "I", SetInt32, AddInt32);
		_CONVERT(CPPTYPE_UINT32, json_int_t, "I", SetUInt32, AddUInt32);
		_CONVERT(CPPTYPE_BOOL, int, "b", SetBool, AddBool);

		case FieldDescriptor::CPPTYPE_STRING: {
			if (!json_is_string(jf))
				throw j2pb_error(field, "Not a string");
			const char * value = json_string_value(jf);
			if(field->type() == FieldDescriptor::TYPE_BYTES)
				_SET_OR_ADD(SetString, AddString, b64_decode(value));
			else
				_SET_OR_ADD(SetString, AddString, value);
			break;
		}
		case FieldDescriptor::CPPTYPE_MESSAGE: {
			Message *mf = (repeated)?
				ref->AddMessage(&msg, field):
				ref->MutableMessage(&msg, field);
			_json2pb(*mf, jf);
			break;
		}
		case FieldDescriptor::CPPTYPE_ENUM: {
			const EnumDescriptor *ed = field->enum_type();
			const EnumValueDescriptor *ev = 0;
			if (json_is_integer(jf)) {
				ev = ed->FindValueByNumber(json_integer_value(jf));
			} else if (json_is_string(jf)) {
				ev = ed->FindValueByName(json_string_value(jf));
			} else
				throw j2pb_error(field, "Not an integer or string");
			if (!ev)
				throw j2pb_error(field, "Enum value not found");
			_SET_OR_ADD(SetEnum, AddEnum, ev);
			break;
		}
		default:
			break;
	}
}

int json2pb(Json::Value const *root, google::protobuf::Message *msg)
{
	const Descriptor *d = msg->GetDescriptor();
	const Reflection *ref = msg->GetReflection();
	if (!d || !ref) return -1;

	for (typeof(root->begin()) it = root->begin(), iend = root->end();
            it != iend; ++it)
	{
		const char *name = it->memberName();

		const FieldDescriptor *field = d->FindFieldByName(name);
		if (!field)
			field = ref->FindKnownExtensionByName(name);

		if (!field) return -2;

		int r = 0;
		if (field->is_repeated()) {
			if (!json_is_array(jf))
				throw j2pb_error(field, "Not array");
			for (size_t j = 0; j < json_array_size(jf); j++)
				_json2field(msg, field, json_array_get(jf, j));
		} else
			_json2field(msg, field, jf);
	}
}

void json2pb(Message &msg, const char *buf, size_t size)
{
	json_t *root;
	json_error_t error;

	root = json_loadb(buf, size, 0, &error);

	if (!root)
		throw j2pb_error(std::string("Load failed: ") + error.text);

	json_autoptr _auto(root);

	if (!json_is_object(root))
		throw j2pb_error("Malformed JSON: not an object");

	_json2pb(msg, root);
}

int json_dump_std_string(const char *buf, size_t size, void *data)
{
	std::string *s = (std::string *) data;
	s->append(buf, size);
	return 0;
}

std::string pb2json(const Message &msg)
{
	std::string r;

	json_t *root = _pb2json(msg);
	json_dump_callback(root, json_dump_std_string, &r, 0);
	return r;
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

