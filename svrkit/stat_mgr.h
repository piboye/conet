/*
 * =====================================================================================
 *
 *       Filename:  stat_mgr.h
 *
 *    Description:  
 *
 *        Version:  1.0
 *        Created:  2015年01月30日 10时53分06秒
 *       Revision:  none
 *       Compiler:  gcc
 *
 *         Author:  piboye
 *   Organization:  
 *
 * =====================================================================================
 */
#ifndef __CONET_STAT_MGR_H__
#define __CONET_STAT_MGR_H__

#include "google/protobuf/message.h"
#include <string>
#include "base/list.h"
#include <map>

namespace conet
{
    typedef google::protobuf::Message *get_stat_func_t(void *);
    struct StatItem
    {
        list_head link;
        get_stat_func_t * get_func;
        std::string name;
    };

    struct StatMgr
    {
        list_head m_list; // 统计列表
        std::map<std::string, StatItem *> m_stat_items;
        int m_stat_interval; /* 统计间隔， 单位是秒*/

        // 注册统计
        bool reg(std::string const &name, get_stat_func_t *get_stat);

        // 注销统计
        bool unreg(std::string const &name);

        void start(int interval/*secnods*/);
        void stop();
    };
}

#endif /* end of include guard */
