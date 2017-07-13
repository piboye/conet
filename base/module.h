#ifndef __MODULE_H__
#define __MODULE_H__

#include "list.h"
#include <map>
#include <string>
#include <vector>

namespace conet
{
namespace module
{

struct module_node_t;

typedef int (*module_cb_t)(module_node_t *);

struct module_func_item
{
    module_cb_t func;
    list_head link_to;
    char const * file_name;
    int line_no;
};


struct module_node_t
{
    char const *file_name;
    int line_no;
    char const *module_name;
    int init_called;
    int fin_called;

    std::vector<std::string> depend_names;
    std::vector<module_node_t*> depend_list;
    list_head init_list;
    list_head fin_list;
    list_head link_to;

    module_node_t();
    ~module_node_t();
};

#define MODULE_LEVEL_NUM 20

class module_mgr_t
{
public:
    std::map<std::string, module_node_t* > m_module_maps;
    std::vector<module_node_t *> m_fin_queue;

    list_head m_level[MODULE_LEVEL_NUM];

    enum{
        NOSTART = 0,
        BEGIN_CACL_DEPS = 1,
        END_CACL_DEPS = 2,

        BEGIN_CALL_INIT = 3,
        END_INIT = 4,
        ERROR_INIT = 5,

        BEGIN_CALL_FIN = 6,
        END_CALL_FIN = 7,

        END = 100
    };

    int m_status;
    int m_calc_called;
    int m_init_called;
    int m_fin_called;

    int m_module_num;
    int m_init_succ_num;
    int m_fin_succ_num;

    int *m_argc;
    char *** m_argv;

    module_mgr_t();

    ~module_mgr_t();

    static module_mgr_t & instance();

    int reg_module(module_node_t *n, int level);

    int init_all_modules();
    int init_all_modules(int *argc, char *** argv);

    int fin_all_modules();

private:

    // 计算模块的依赖关系
    int calc_all_deps();

    int call_all_init();

    int call_module_init(module_node_t *n);

    int call_module_fin(module_node_t *n);
};

#define InitAllModule(argc, argv) \
    conet::module::module_mgr_t::instance().init_all_modules()

#define FinAllModule() \
    conet::module::module_mgr_t::instance().fin_all_modules()


#define MODULE_INIT() \
    static conet::module::module_func_item g_module_init_item_var_##__LINE__##_; \
    static int g_module_init_help_func_##__LINE__##_(); \
    static int g_module_init_func_##__LINE__##_(conet::module::module_node_t *); \
    static int g_module_init_item_help_var_##__LINE__##_ = g_module_init_help_func_##__LINE__##_(); \
    static int g_module_init_help_func_##__LINE__##_() { \
        g_module_init_item_var_##__LINE__##_.func = & g_module_init_func_##__LINE__##_; \
        g_module_init_item_var_##__LINE__##_.file_name = __FILE__; \
        g_module_init_item_var_##__LINE__##_.line_no = __LINE__; \
        list_add_tail(& g_module_init_item_var_##__LINE__##_.link_to, &g_module_node_var_.init_list); \
        return 1;  \
    } \
    static int g_module_init_func_##__LINE__##_(conet::module::module_node_t *self) \

#define MODULE_FIN() \
    static conet::module::module_func_item g_module_fin_item_var_##__LINE__##_; \
    static int g_module_fin_help_func_##__LINE__##_(); \
    static int g_module_fin_func_##__LINE__##_(conet::module::module_node_t *); \
    static int g_module_fin_item_help_var_##__LINE__##_ = g_module_fin_help_func_##__LINE__##_(); \
    static int g_module_fin_help_func_##__LINE__##_() { \
        g_module_fin_item_var_##__LINE__##_.func = & g_module_fin_func_##__LINE__##_; \
        g_module_fin_item_var_##__LINE__##_.file_name = __FILE__; \
        g_module_fin_item_var_##__LINE__##_.line_no = __LINE__; \
        list_add_tail(& g_module_fin_item_var_##__LINE__##_.link_to, &g_module_node_var_.fin_list); \
        return 1;  \
    } \
    static int g_module_fin_func_##__LINE__##_(conet::module::module_node_t *self) \


#define DEFINE_MODULE_HELP(name, level) \
    static conet::module::module_node_t g_module_node_var_; \
    static int g_define_module_help_func_##__LINE__##_(){ \
        conet::module::module_node_t & m =  g_module_node_var_; \
        m.file_name = __FILE__; \
        m.line_no = __LINE__; \
        m.module_name = name; \
        conet::module::module_mgr_t::instance().reg_module(&m, level); \
        return 1; \
    } \
    static int g_define_module_help_var_##__LINE__##_ = g_define_module_help_func_##__LINE__##_(); \
    MODULE_INIT() \


#define DEFINE_MODULE(name) DEFINE_MODULE_HELP(#name, 10)

#define DEFINE_ANON_MODULE() DEFINE_MODULE_HELP("", 10)

#define DEFINE_ANON_MODULE_WITH_LEVEL(level) DEFINE_MODULE_HELP("", level)

#define DEFINE_MODULE_WITH_LEVEL(name, level) DEFINE_MODULE_HELP(#name, level)

#define USING_MODULE(name)  \
    static int g_using_module_help_func_##__LINE__##_(){ \
        conet::module::module_node_t & m =  g_module_node_var_; \
        m.depend_names.push_back(#name); \
        return 1; \
    } \
    static int g_using_module_help_var_##__LINE__##_ = g_using_module_help_func_##__LINE__##_() \

}
}
#endif //__MODULE_H__
