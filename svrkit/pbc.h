#ifndef _PBC_H_
#define _PBC_H_

#include <stdint.h>
#include <stddef.h>
#include <string.h>

static inline uint64_t pb_int2uint(int64_t s)
{
    if (s < 0)
        return (uint64_t)(~(s << 1));
    else
        return (uint64_t)(s << 1);
}

static inline int64_t pb_uint2int(uint64_t u)
{
    return (u & 1)? (int64_t)(~(u >> 1)) : (int64_t)(u >> 1);
}



/* Wire types. */
typedef enum {
    PB_WT_VARINT = 0,
    PB_WT_FIXED64  = 1,
    PB_WT_STRING = 2,
    PB_WT_FIXED32  = 5
} pb_wire_type_t;


typedef union {
    uint32_t i32;
    uint64_t i64;
    struct
    {
        uint32_t len;
        char *data;
    } str;
} pb_field_val_t;

typedef struct _pb_field_t {

    uint32_t tag;
    char eom; // set to 1 if end-of-message
    pb_wire_type_t type;
    pb_field_val_t val;

    // msg start
    const char *start;
    uint32_t size;

    const char *ptr; // cur unhandled position
    int left; // unhandled length

    const char *errmsg;
}pb_field_t;

// functions for iteration through pb fields
pb_field_t *pb_begin(pb_field_t *f, const void *data, int dlen);
pb_field_t *pb_begin_delimited(pb_field_t *f, const void *data, int dlen);
pb_field_t *pb_next(pb_field_t *f);
static inline pb_field_t *pb_end() { return NULL; }
int pb_skip_field(pb_field_t *f, pb_wire_type_t wire_type); // skip one specific field



typedef struct 
{
    char *start; // buffer start
    size_t size; // buffer size

    char *ptr; // cur unwritten pointer
    int left; // unwritten length

    const char *errmsg;
}pb_buff_t;

static inline int pb_init_buff(pb_buff_t *b, void *buf, size_t sz)
{
    b->start = b->ptr = (char*)buf;
    b->size = b->left = sz;
    return 0;
}
int pb_add_field(pb_buff_t *b, const pb_field_t *field);
int pb_add_field_val(pb_buff_t *b, uint32_t tag, pb_wire_type_t type, const pb_field_val_t *val);
int pb_add_varint(pb_buff_t *b, uint32_t tag, uint64_t value);
int pb_add_string(pb_buff_t *b, uint32_t tag, const void *buffer, size_t size);
int pb_add_string_head(pb_buff_t *b, uint32_t tag, size_t size); // add only tag/type/length
int pb_add_fixed32(pb_buff_t *b, uint32_t tag, uint32_t value);
int pb_add_fixed64(pb_buff_t *b, uint32_t tag, uint64_t value);
int pb_add_user(pb_buff_t *b, const void *buf, size_t count); // add user buffer
static inline int pb_get_encoded_length(pb_buff_t *b) { return b->size - b->left; }



#define PB_GET_ERROR(p) ((p)->errmsg ? (p)->errmsg : "(none)")

static inline int pb_eval_vint_length(uint64_t value)
{
    int eval_length = 0;
    if(value==0) return 1;
    while (value) {
        value >>= 7;
        eval_length++;
    }
    return eval_length;
}

static inline int pb_eval_tag_length(pb_wire_type_t wiretype, uint32_t field_number)
{
    return pb_eval_vint_length(wiretype | (field_number << 3));
}

// functions for evaluating a field's length
static inline int pb_eval_field_length(uint32_t tag, pb_wire_type_t type, const pb_field_val_t *val)
{
    switch (type)
    {
        case PB_WT_VARINT:  return pb_eval_tag_length(type, tag) + pb_eval_vint_length(val->i64);
        case PB_WT_FIXED64: return pb_eval_tag_length(type, tag) + 8;
        case PB_WT_FIXED32: return pb_eval_tag_length(type, tag) + 4;
        default:
        case PB_WT_STRING:  return pb_eval_tag_length(type, tag) + pb_eval_vint_length(val->str.len) + val->str.len;
    }
}

#endif

