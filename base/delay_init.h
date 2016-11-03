#ifndef __DELAY_INIT_H__
#define __DELAY_INIT_H__

#include "list.h"

namespace delay_init {
typedef int (*callback)(void);

struct file_node {
    char const *file_name;
    int called;
    list_head init_list;
    list_head depend_list;
    list_head link_to_root;
};

#define DELAY_INIT_FILE_NODE_INIT(name) \
        {\
            __BASE_FILE__,\
            0,\
            LIST_HEAD_INIT(name.init_list),\
            LIST_HEAD_INIT(name.depend_list),\
            LIST_HEAD_INIT(name.link_to_root)\
        }

#define DELAY_INIT_FILE_NODE(name) 	delay_init::file_node name = DELAY_INIT_FILE_NODE_INIT(name)

struct delay_init_struct {
    list_head link_to_file;
    callback init;
    char const *info;
    int called;
};
#define DELAY_INIT_STRUCT_INIT(name) \
        {LIST_HEAD_INIT(name.link_to_file), 0, 0, 0}

#define DELAY_INIT_STRUCT(name)  delay_init::delay_init_struct name=DELAY_INIT_STRUCT_INIT(name)

struct delay_init_depend_struct {
    list_head link_to_depend;
    file_node **point_file_node;
};

#define DELAY_INIT_DEPEND_STRUCT_INIT(name)\
        {LIST_HEAD_INIT(name.link_to_depend), 0}

#define DELAY_INIT_DEPEND_STRUCT(name)  delay_init::delay_init_depend_struct name = DELAY_INIT_DEPEND_STRUCT_INIT(name)

#define INIT_LEVEL_NUM 10
#define INIT_LOG_LEVEL 3
#define INIT_CONF_LEVEL 2
#define INIT_CONF_IOC_LEVEL 1
#define INIT_FIRST_LEVEL 0
#define INIT_LAST_LEVEL (INIT_LEVEL_NUM-1)

extern list_head g_delay_init_list_head[INIT_LEVEL_NUM];

int call_all_level();

int call_in_level(int level);

extern int failed_cnt;
extern int success_cnt;
extern int total_cnt;
extern int called_cnt;
}

#define MACRO_NAME_CONCAT2(a,b) MACRO_NAME_CONCAT2_PRITIVE(a,b)
#define MACRO_NAME_CONCAT2_PRITIVE(a,b) a##b
#define STRING_NAME(name) STRING_NAME_IMPL(name)
#define STRING_NAME_IMPL(name) #name

inline
int __one_call_call_fun__(void(*fun)(void)) {
    fun();
    return 1;
}
#define ONCE_CALL_FUNC(name) MACRO_NAME_CONCAT2(__func_once_call_ ,name)
#define ONCE_CALL_VAR(name) MACRO_NAME_CONCAT2(__var_once_call_, name)
#define ONCE_CALL_WITH(name) \
		 static void ONCE_CALL_FUNC(name)(void); \
		 static int __attribute__((__unused__)) ONCE_CALL_VAR(name) = __one_call_call_fun__(&ONCE_CALL_FUNC(name)); \
		 static void ONCE_CALL_FUNC(name)(void)

#define ONCE_CALL ONCE_CALL_WITH(MACRO_NAME_CONCAT2(__COUNTER__,__LINE__))

static DELAY_INIT_FILE_NODE(_this_file_node_delay_init_);

ONCE_CALL_WITH(delay_init_add_file_to_root)
{
    list_add_tail(&_this_file_node_delay_init_.link_to_root,
            &delay_init::g_delay_init_list_head[INIT_LAST_LEVEL]);
}

#define DELAY_INIT_IN_LEVEL(level) \
ONCE_CALL \
{\
	list_move_tail(&_this_file_node_delay_init_.link_to_root, &delay_init::g_delay_init_list_head[level]);\
}

#define DELAY_INIT_NAME(name) MACRO_NAME_CONCAT2(MACRO_NAME_CONCAT2(__delay_init_def_, name), __LINE__)
#define DELAY_INIT() \
		 static int DELAY_INIT_NAME(init_fun)(void); \
		 static DELAY_INIT_STRUCT(DELAY_INIT_NAME(init_node));\
		 ONCE_CALL { \
		     DELAY_INIT_NAME(init_node).init =&DELAY_INIT_NAME(init_fun);\
		     DELAY_INIT_NAME(init_node).info = \
		        "delay init name:" __FILE__ "_" STRING_NAME(__LINE__)\
		        " call in:" __FILE__ " line:" STRING_NAME(__LINE__); \
			list_add_tail(&DELAY_INIT_NAME(init_node).link_to_file, &_this_file_node_delay_init_.init_list);\
			 return ;\
		}\
		static int DELAY_INIT_NAME(init_fun)(void)

#define DELAY_INIT_EXPORT_NAME(name) MACRO_NAME_CONCAT2(__delay_init_export_name_, name)

#define DELAY_INIT_EXPORT(name) \
		delay_init::file_node * DELAY_INIT_EXPORT_NAME(name) = & _this_file_node_delay_init_;

#define DEPEND_ON_DELAY_INIT_NAME(name) MACRO_NAME_CONCAT2(name, MACRO_NAME_CONCAT2(__delay_init_depend_struct_, __LINE__))

#define DEPEND_ON_DELAY_INIT(name)\
		extern delay_init::file_node * DELAY_INIT_EXPORT_NAME(name);\
		static DELAY_INIT_DEPEND_STRUCT(DEPEND_ON_DELAY_INIT_NAME(_struct_));\
		ONCE_CALL {\
		    DEPEND_ON_DELAY_INIT_NAME(_struct_).point_file_node = &DELAY_INIT_EXPORT_NAME(name);\
			list_add_tail(&DEPEND_ON_DELAY_INIT_NAME(_struct_).link_to_depend, &_this_file_node_delay_init_.depend_list);\
		}

#endif //__DELAY_INIT_H__
