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

#include "thirdparty/glog/logging.h"
#include "thirdparty/gflags/gflags.h"
#include "base/delay_init.h"
#include "base/net_tool.h"
#include "server_common.h"
#include "server_builder.h"
#include "svrkit/rpc_conf.pb.h"
#include "svrkit/static_resource.h"
#include "base/pb2json.h"


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
    data.resize(RESOURCE_svrkit_default_server_conf_len+500);
    int size = snprintf((char *)data.c_str(), data.size(), 
            RESOURCE_svrkit_default_server_conf,
            FLAGS_ip.c_str(),
            FLAGS_port,
            (int)FLAGS_duplex,
            FLAGS_thread_num
    );
    data.resize(size);
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
        LOG(ERROR)<<"open conf file "<<conf_file<<" failed!";
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

    // 清理glog 库内存
    google::ShutdownGoogleLogging();

    // 清理gflags 库内存
    gflags::ShutDownCommandLineFlags();
}

static int call_delay_init()
{
    // delay init
    delay_init::call_all_level();
    LOG(INFO)<<"delay init total:"<<delay_init::total_cnt
        <<" success:"<<delay_init::success_cnt
        <<", failed:"<<delay_init::failed_cnt;

    if(delay_init::failed_cnt>0)
    {
        LOG(ERROR)<<"delay init failed, failed num:"<<delay_init::failed_cnt;
        return 1;
    }
    return 0;
}

int main(int argc, char * argv[])
{
    int ret = 0;

    ret = gflags::ParseCommandLineFlags(&argc, &argv, false);
    google::InitGoogleLogging(argv[0]);


    GOOGLE_PROTOBUF_VERIFY_VERSION;

    signal(SIGINT, sig_exit);
    signal(SIGPIPE, SIG_IGN);

    mallopt(M_MMAP_THRESHOLD, 1024*1024); // 1MB，防止频繁mmap
    mallopt(M_TRIM_THRESHOLD, 8*1024*1024); // 8MB，防止频繁brk

    ret = call_delay_init();
    if (ret)
    {
        return 1;
    }

    if (conet::can_reuse_port()) {
        LOG(INFO)<<"can reuse port, very_good";
        //fprintf(stderr, "can resue port, very good!\n");
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
        LOG(ERROR)<<"parse conf file failed! error:"<<errmsg<<
            " ret:"<<ret;
        return 2;
    }

    g_server_container = ServerBuilder::build(conf);
    if (NULL == g_server_container)
    {
        LOG(ERROR)<<"build server failed!";
        return 3;
    }

    g_server_container->start();
    while(!get_server_stop_flag())
    {
       sleep(1); 
    }

    LOG(INFO)<<"server ready exit!";
    ret = g_server_container->stop(FLAGS_stop_wait_seconds);

    delete g_server_container;
    g_server_container = NULL;

    conet::call_server_fini_func();

    fini_google_lib();

    return 0;
}



