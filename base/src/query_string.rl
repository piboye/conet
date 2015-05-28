#include <map>
#include <string>
#include <assert.h>
#include <stdlib.h>
#include <ctype.h>
#include <string.h>

#include "query_string.h"
#include "url_encode.h"
#include "ref_str.h"
#include "string2number.h"
#include "thirdparty/google/protobuf/descriptor.h"

%%{
  machine query_string;

  action start_name {
      mark = fpc;
  }

  action end_name {
      DO_NAME(mark, (fpc-mark));
  }

  action start_value {
      mark = fpc;
  }
  action end_value {
      DO_VALUE(mark, (fpc-mark));
  }

  name = (any -- "=&")+ >start_name %end_name;
  value = (any -- "=&")+ >start_value %end_value;
  main := (name "=" value)?("&" name "=" value)*;

}%%

namespace conet
{

/** Data **/
%% write data;

int parse_query_string(char const *buf, size_t len, std::map<std::string, std::string> *param)
{
    int cs = 0;
    %% write init;

    char const *p, *pe;
    char const *mark=NULL;

    p = buf;
    pe = buf + len;
    const char* eof = pe;

    std::string name;
    std::string value;

#define DO_NAME(start, len) name.assign(start, len)

#define DO_VALUE(start, len) \
      value.clear(); \
      url_decode(start, len, &value); \
      param->insert(std::make_pair(name, value)) \


    %% write exec;

    if (cs == query_string_error || cs <query_string_first_final) {
        return -1;
    }

#undef DO_NAME
#undef DO_VALUE

    return 0;
}


int query_string_to_json(char const *buf, size_t len, Json::Value *root)
{
    int cs = 0;

    %% write init;

    std::string name;
    std::string value;

    char const *p, *pe;
    char const *mark=NULL;

    p = buf;
    pe = buf + len;
    const char* eof = pe;

#define DO_NAME(start, len) name.assign(start, len)

#define DO_VALUE(start, len2) \
    value.clear(); \
    url_decode(start, len2, &value); \
    { \
        Json::Value v2(value); \
        (*root)[name].swap(v2); \
    } \

    %% write exec;

    if (cs == query_string_error || cs <query_string_first_final) {
        return -1;
    }

#undef DO_NAME
#undef DO_VALUE

    return 0;
}

int query_string_to_msg(char const *buf, size_t len, google::protobuf::Message *msg)
{
    int cs = 0;

    const google::protobuf::Reflection* reflection = msg->GetReflection();
    const google::protobuf::Descriptor* descriptor = msg->GetDescriptor();

    %% write init;

    std::string name;
    std::string value;

    char const *p, *pe;
    char const *mark=NULL;

    p = buf;
    pe = buf + len;
    const char* eof = pe;

#define DO_NAME(start, len) name.assign(start, len)

#define CASE_FIELD_TYPE(cpptype, method, type2) \
    case google::protobuf::FieldDescriptor::CPPTYPE_##cpptype: { \
        type2 v2 = 0; \
        string2number<type2>(value, &v2); \
        reflection->Set##method(msg, field, v2); \
        break; \
    } \


#define DO_VALUE(start, len2) \
    value.clear(); \
    value.assign(start, len2); \
    do { \
        const google::protobuf::FieldDescriptor* field = descriptor->FindFieldByName(name); \
        if (NULL == field) break; \
        switch (field->cpp_type()) \
        { \
            CASE_FIELD_TYPE(INT64,  Int64,  int64_t); \
            CASE_FIELD_TYPE(UINT64, UInt64, uint64_t); \
            CASE_FIELD_TYPE(INT32,  Int32,  int32_t); \
            CASE_FIELD_TYPE(UINT32, UInt32, uint32_t); \
            CASE_FIELD_TYPE(FLOAT,  Float,  float); \
            CASE_FIELD_TYPE(DOUBLE, Double, double); \
            CASE_FIELD_TYPE(BOOL,   Bool,   bool); \
            case google::protobuf::FieldDescriptor::CPPTYPE_ENUM: \
            { \
                int64_t v2 = 0; \
                string2number<int64_t>(value, &v2); \
                const google::protobuf::EnumValueDescriptor * enum_value_descriptor = \
                field->enum_type()->FindValueByNumber(v2); \
                if (enum_value_descriptor) { \
                    reflection->SetEnum(msg, field, enum_value_descriptor); \
                } else { \
                } \
                break; \
            } \
            case google::protobuf::FieldDescriptor::CPPTYPE_STRING: { \
                value.clear(); \
                url_decode(start, len2, &value); \
                reflection->SetString(msg, field, value); \
                break; \
            } \
            default: \
            { \
                break; \
            } \
        } \
    } while(0) \


    %% write exec;

    if (cs == query_string_error || cs <query_string_first_final) {
        return -1;
    }

#undef DO_NAME
#undef DO_VALUE
#undef DO_FILED_TYPE

    return 0;
}

}

