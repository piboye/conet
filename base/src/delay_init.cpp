#include <stdio.h>
#include "delay_init.h"
#include "macro_help.h"
#include "plog.h"


namespace delay_init {

int failed_cnt = 0;
int success_cnt = 0;
int total_cnt = 0;
int called_cnt = 0;

#define INIT_LIST_HEAD_IMPL(s, index, name) BOOST_PP_COMMA_IF(index) LIST_HEAD_INIT(name[index])

list_head g_delay_init_list_head[INIT_LEVEL_NUM] = {
    BOOST_PP_REPEAT(INIT_LEVEL_NUM, INIT_LIST_HEAD_IMPL, g_delay_init_list_head)
};

static int call_file_list(file_node *file_list) {
    if (file_list->called) {
        return 0;
    }
    PLOG_DEBUG("delay_init in file:", file_list->file_name);
    int ret = 0;
    list_head *q = NULL;
    list_for_each(q, &file_list->depend_list) {
        delay_init_depend_struct *depend =
                container_of(q, delay_init_depend_struct, link_to_depend);
        if (*(depend->point_file_node)) {
            ret += call_file_list(*(depend->point_file_node));
        }
    }

    list_for_each(q, &file_list->init_list) {
        delay_init_struct *st =
                container_of(q, delay_init_struct, link_to_file);
        callback init = st->init;
        if (init) {
            if (0 == st->called) {
                PLOG_DEBUG("call delay init func:", st->info);
                ret += init();
                ++st->called;
                ++called_cnt;
            }
            ++success_cnt;
        } else {
            ++failed_cnt;
        }
        ++total_cnt;
    }
    ++(file_list->called);
    return ret;
}

int call_in_level(int level) {
    if (level < 0 || INIT_LEVEL_NUM - 1 < level) {
        PLOG_ERROR("error init ", (level), " group");
        return -1;
    }
    PLOG_DEBUG("delay_init in ", (level));
    int ret = 0;
    list_head *q = NULL;
    list_for_each(q, &g_delay_init_list_head[level]) {
        file_node * file_list = container_of(q, file_node, link_to_root);
        ret += call_file_list(file_list);
    }
    return ret;
}

int call_all_level() {
    int ret = 0;
    for (int i = 0; i < INIT_LEVEL_NUM; ++i) {
        ret += call_in_level(i);
    }
    return ret;
}

}


#ifdef ENABLE_DELAY_INIT_CPP_LOG
#undef LOG
#undef ENABLE_DELAY_INIT_CPP_LOG
#endif //ENABLE_DELAY_INIT_CPP_LOG
