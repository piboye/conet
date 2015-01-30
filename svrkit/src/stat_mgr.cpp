/*
 * =====================================================================================
 *
 *       Filename:  stat_mgr.cpp
 *
 *    Description:  
 *
 *        Version:  1.0
 *        Created:  2015年01月30日 10时59分16秒
 *       Revision:  none
 *       Compiler:  gcc
 *
 *         Author:  piboye
 *   Organization:  
 *
 * =====================================================================================
 */
#include <stdlib.h>
#include "stat_mgr.h"
#include "base/pb2sqlite.h"
#include "tls.h"
#include "glog/logging.h"
#include "base/auto_var.h"

namespace conet
{

bool StatMgr::reg(std::string const &name, get_stat_func_t * get_stat)
{
    if (m_stat_items.find(name) != m_stat_items.end())
    {
        LOG(ERROR)<<"conflict stat item [name:"<<name<<"]";
        return false;
    }
    StatItem * item = new StatItem();
    item->name = name;
    item->get_func = get_stat;
    INIT_LIST_HEAD(&item->link);
    list_add_tail(&item->link, &this->m_list);
    m_stat_items.insert(std::make_pair(name, item));
    return true;
}


bool StatMgr::unreg(std::string const &name)
{
    AUTO_VAR(it , = , m_stat_items.find(name));
    if (it == m_stat_items.end()) {
        LOG(ERROR)<<"noexist stat item [name:"<<name<<"]";
        return false;
    }

    StatItem *item = it->second;
    list_del_init(&item->link);
    m_stat_items.erase(it);
    return true;
}

void StatMgr::start(int interval)
{
    m_stat_interval = interval;
}
void StatMgr::stop()
{

}

}

