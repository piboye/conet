#include "../module.h"
#include "../auto_var.h"
#include "../plog.h"
#include "../list.h"

namespace conet
{

namespace module
{

module_node_t::module_node_t()
{
    file_name = NULL;
    module_name = NULL;
    line_no = 0;
    init_called = 0;
    fin_called = 0;

    INIT_LIST_HEAD(&init_list);
    INIT_LIST_HEAD(&fin_list);
    INIT_LIST_HEAD(&link_to);
}

module_node_t::~module_node_t()
{

}

module_mgr_t::module_mgr_t()
{
    m_status = NOSTART;
    m_init_called = 0;
    m_fin_called = 0;
    m_calc_called = 0;
    m_module_num = 0;
    m_init_succ_num = 0;
    m_fin_succ_num = 0;
    m_argc = 0;
    m_argv = NULL;

    int num = sizeof(m_level)/sizeof(list_head);
    for(int i=0; i<num; ++i)
    {
        INIT_LIST_HEAD(m_level+i);
    }

}

module_mgr_t::~module_mgr_t()
{
    this->fin_all_modules();
}


static module_mgr_t *g_module_mgr = NULL;

static void delete_module_mgr()
{
    delete g_module_mgr;
}

module_mgr_t & module_mgr_t::instance()
{
    if (NULL == g_module_mgr)
    {
        module_mgr_t * mgr = new module_mgr_t();

        g_module_mgr = mgr;
        atexit(delete_module_mgr);
    }
    return *g_module_mgr;
}

int module_mgr_t::reg_module(module_node_t *n, int level)
{
    if (level >= MODULE_LEVEL_NUM)
    {
        PLOG_ERROR("invalid module level, "
                "[max_level", MODULE_LEVEL_NUM-1, "]"
                "[level", level, "] "
                "[module=", n->module_name, "] "
                "[file=", n->file_name, "] "
                "[line=", n->line_no, "] "
                );
        return -1;
    }

    std::string name  = n->module_name;
    if (!name.empty())
    {
        AUTO_VAR(it, =, m_module_maps.find(name));
        if (it != m_module_maps.end())
        {
            PLOG_ERROR("duplicate reg [module=", name, "], prev in [file=", it->second->file_name, "] [line=", it->second->line_no, "]");
            return -2;
        }
        m_module_maps.insert(std::make_pair(name, n));
    }

    list_add_tail(&n->link_to, &this->m_level[level]);
    ++m_module_num;

    return 0;
}

int module_mgr_t::calc_all_deps()
{
    for(int i=0; i<MODULE_LEVEL_NUM; ++i)
    {
        module_node_t *p = NULL;
        list_for_each_entry(p, &m_level[i], link_to)
        {
            for(size_t j=0; j<p->depend_names.size(); ++j)
            {
                std::string const &dep_name = p->depend_names[j];
                AUTO_VAR(it, =, m_module_maps.find(dep_name));
                if (it==m_module_maps.end())
                {
                    PLOG_ERROR("depend noexist module, "
                            "[module=", p->module_name, "]"
                            "[dep_module=", dep_name, "]"
                        );
                    return -1;
                }

                module_node_t * dep_m = it->second;

                p->depend_list.push_back(dep_m);
            }
        }
    }

    return 0;
}

int module_mgr_t::call_module_init(module_node_t *n)
{
    if (n->init_called)
    {
        return 0;
    }

    int ret = 0;
    // 调用所有的依赖模块的 init
    for(size_t j=0; j<n->depend_list.size(); ++j)
    {
        module_node_t * dep_m = n->depend_list[j];
        ret = this->call_module_init(dep_m);
        if (ret != 0)
        {
            PLOG_ERROR("call depend module init failed,"
                    " [module=", n->module_name, "]"
                    " [dep_module=", dep_m->module_name, "]"
                    " [ret=",ret, "]"
                );
            return -1;
        }
    }

    // 调用 init 函数
    module_func_item *item=NULL;
    list_for_each_entry(item, &n->init_list, link_to)
    {
        ret = item->func(n);
        if (ret != 0)
        {
            PLOG_ERROR("call module init func failed,"
                    " [module=", n->module_name, "]"
                    " [init.filename=", item->file_name, "]"
                    " [init.lineno=", item->line_no, "]"
                    " [ret=",ret, "]"
                    );
            return -1;
        }
    }

    this->m_fin_queue.push_back(n);

    n->init_called = 1;

    ++ this->m_init_succ_num ;

    return 0;
}

int module_mgr_t::call_all_init()
{
    int ret = 0;
    for(int i=0; i<MODULE_LEVEL_NUM; ++i)
    {
        module_node_t *p = NULL;
        list_for_each_entry(p, &m_level[i], link_to)
        {
            ret = this->call_module_init(p);
            if (ret != 0)
            {
                return -1;
            }
        }
    }

    return 0;
}

int module_mgr_t::init_all_modules(int *argc, char ***argv)
{
    this->m_argc = argc;
    this->m_argv = argv;
    return init_all_modules();
}

int module_mgr_t::init_all_modules()
{
    int ret = 0;
    if (m_init_called || m_fin_called)
    {
        return 0;
    }

    if (!m_calc_called)
    {
        m_calc_called = 1;
        ret = calc_all_deps();
        if (ret != 0)
        {
            return -1;
        }
    }

    if (!m_init_called)
    {
        m_init_called = 1;
        ret = call_all_init();
        if (ret != 0)
        {
            return -2;
        }
    }

    return 0;
}

int module_mgr_t::call_module_fin(module_node_t *n)
{
    if (n->fin_called)
    {
        return 0;
    }

    int ret1 = 0;
    n->fin_called = 1;
    module_func_item *item=NULL;
    list_for_each_entry_reverse(item, &n->fin_list, link_to)
    {
        int ret = item->func(n);
        if (ret != 0)
        {
            PLOG_ERROR("call module fin func failed,"
                    " [module=", n->module_name, "]"
                    " [fin.filename=", item->file_name, "]"
                    " [fin.lineno=", item->line_no, "]"
                    " [ret=",ret, "]"
                    );
            ret1 = ret;
        }
    }


    return ret1;
}

int module_mgr_t::fin_all_modules()
{
    if (0==m_init_called)
    {
        return -1;
    }

    if (m_fin_called)
    {
        return 0;
    }

    m_fin_called = 1;

    for (int i = (int) m_fin_queue.size()-1; i>=0; --i)
    {
        module_node_t * n = m_fin_queue[i];
        int ret = this->call_module_fin(n);
        if (ret == 0)
        {
            ++ this->m_fin_succ_num;
        }
    }

    return 0;
}

}

}
