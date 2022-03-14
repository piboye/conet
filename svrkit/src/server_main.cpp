/*
 * =====================================================================================
 *
 *       Filename:  server_main.cpp
 *
 *    Description:  
 *
 *        Version:  1.0
 *        Created:  2014年09月15日 23时20分47秒
 *       Revision:  none
 *       Compiler:  gcc
 *
 *         Author:  piboye
 *   Organization:  
 *
 * =====================================================================================
 */

#include <stdlib.h>
#include <stdio.h>
#include <errno.h>
#include <string.h>
#include <unistd.h>
#include <signal.h>
#include <malloc.h>
#include <sys/wait.h>
#include <sys/types.h>
#include <map>
#include <vector>
#include <fstream>

#include "thirdparty/gflags/gflags.h"
#include "base/net_tool.h"
#include "server_common.h"
#include "server_builder.h"
#include "svrkit/rpc_conf.pb.h"
#include "svrkit/static_resource.h"
#include "base/pb2json.h"
#include "base/defer.h"
#include "core/conet_all.h"
#include "base/string_tpl.h"
#include "base/string2number.h"
#include "base/plog.h"
#include "base/module.h"


namespace conet
{

DEFINE_int32(stop_wait_seconds, 10, "server stop wait seconds");
DEFINE_string(conf, "", "server conf");
DEFINE_string(ip, "0.0.0.0", "server ip");
DEFINE_int32(port, 12314, "server port");
DEFINE_int32(thread_num, 0, "thread num");
DEFINE_bool(duplex, false, "duplex in tcp");


}


using namespace conet;

static
void sig_exit(int sig)
{
   set_server_stop();
}

using namespace conet;
std::string get_default_conf()
{
    std::string data;
    std::map<std::string, std::string> datas;
    datas["ip"] = FLAGS_ip;
    datas["port"] = number2string(FLAGS_port);
    datas["duplex"] = number2string((int)(FLAGS_duplex));
    datas["thread_num"] = number2string((int)(FLAGS_thread_num));
    int ret =0;

    std::string errmsg;
    ret = string_tpl(RESOURCE_svrkit_default_server_conf, datas, &data, &errmsg);
    if (ret) {
        PLOG_ERROR("load default conf failed, ", (ret, errmsg));
    } else {
        PLOG_INFO("load default [conf=", data, "]");
    }
    return data;
}

static 
int get_conf_data(std::string const & conf_file, std::string *data)
{

    if (conf_file.empty())
    {
        *data =  get_default_conf();
        return 0;
    }

    std::fstream fs;
    fs.open(conf_file.c_str(), std::fstream::in);
    if (!fs.is_open())
    {
        PLOG_ERROR("open conf [file=", conf_file, " failed!");
        return -1;
    }
    std::string line;
    while(std::getline(fs, line))
    {
        *data += line;
    }

    return 0;
}

static conet::ServerContainer * g_server_container=NULL;


static 
void fini_google_lib()
{
    // 清理protobuf 库的内存
    google::protobuf::ShutdownProtobufLibrary();

    // 清理gflags 库内存
    gflags::ShutDownCommandLineFlags();
}

int main(int argc, char * argv[])
{
    int ret = 0;
    conet::init_conet_global_env();
    conet::init_conet_env();

    InitAllModule(argc, argv);

    GOOGLE_PROTOBUF_VERIFY_VERSION;

    signal(SIGINT, sig_exit);
    signal(SIGPIPE, SIG_IGN);

    mallopt(M_MMAP_THRESHOLD, 1024*1024); // 1MB，防止频繁mmap
    mallopt(M_TRIM_THRESHOLD, 8*1024*1024); // 8MB，防止频繁brk

    if (conet::can_reuse_port()) {
        PLOG_INFO("can reuse port, very_good");
    }

    std::string data;
    if (get_conf_data(FLAGS_conf, &data)) {
        return 1;
    }

    ServerConf conf;
    std::string errmsg;
    ret = json2pb(data, &conf, &errmsg);
    if (ret)
    {
        PLOG_ERROR("parse conf file failed! ", (ret, errmsg));
        return 2;
    }



    g_server_container = ServerBuilder::build(conf);
    if (NULL == g_server_container)
    {
        conet::free_conet_env();
        conet::free_conet_global_env();
        PLOG_ERROR("build server failed!");
        return 3;
    }

    CO_RUN((g_server_container), {
        g_server_container->start();
    });

    int exit_finished = 0;
    while(likely((exit_finished< 2)))
    {
        if (unlikely(get_server_stop_flag() && exit_finished == 0))
        {
           exit_finished = 1;
           CO_RUN((exit_finished), {
                PLOG_INFO("server ready exit!");
                int ret = 0;
                ret = g_server_container->stop(FLAGS_stop_wait_seconds);
                if (ret) {
                    PLOG_ERROR("server exit failed, [ret=", ret, "]!");
                } else {
                    PLOG_INFO("server exit success!");
                }
                exit_finished = 2;
           });
        }
        conet::dispatch();
    }


    delete g_server_container;
    g_server_container = NULL;

    conet::free_conet_env();
    conet::free_conet_global_env();


    fini_google_lib();
    return 0;
}
