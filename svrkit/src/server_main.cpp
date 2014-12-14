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

#include "thirdparty/glog/logging.h"
#include "thirdparty/gflags/gflags.h"
#include "base/incl/delay_init.h"
#include "base/incl/net_tool.h"
#include "server_controller.h"
#include "server_common.h"


DEFINE_int32(stop_wait_seconds, 2, "server stop wait seconds");

namespace conet
{


}



using namespace conet;


ServerController *g_server_controller = NULL;

static
void sig_exit(int sig)
{
   set_server_stop();
   if (g_server_controller) g_server_controller->m_stop_flag = 1;
}


int main(int argc, char * argv[])
{
    int ret = 0;
    signal(SIGINT, sig_exit);

    mallopt(M_MMAP_THRESHOLD, 1024*1024); // 1MB，防止频繁mmap 
    mallopt(M_TRIM_THRESHOLD, 8*1024*1024); // 8MB，防止频繁brk 

    ret = google::ParseCommandLineFlags(&argc, &argv, false); 
    google::InitGoogleLogging(argv[0]);

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
    }

    if (conet::can_reuse_port()) {
        LOG(INFO)<<"can reuse port, very_good";
        fprintf(stderr, "can resue port, very good!\n");
    }

    g_server_controller = ServerController::create(); 

    ret = g_server_controller->start();
    g_server_controller->run();
    g_server_controller->stop(FLAGS_stop_wait_seconds);

    delete g_server_controller;
    g_server_controller = NULL;

    conet::call_server_fini_func();
    return 0;
}

