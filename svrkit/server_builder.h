/*
 * =====================================================================================
 *
 *       Filename:  server_builder.cpp
 *
 *    Description:  
 *
 *        Version:  1.0
 *        Created:  2015年02月17日 02时45分28秒
 *       Revision:  none
 *       Compiler:  gcc
 *
 *         Author:  piboye
 *   Organization:  
 *
 * =====================================================================================
 */
#include <stdlib.h>
#include "server_base.h"
#include "rpc_pb_server.h"
#include "svrkit/rpc_conf.pb.h"
#include <pthread.h>

namespace conet
{
class server_group_t;
class server_worker_t
{
public:
    server_worker_t();
    server_group_t * server_group;
    pthread_t tid;
    std::vector<rpc_pb_server_t*> rpc_servers;
    int stop_flag;
    int exit_finsished;
    int exit_seconds;
    cpu_set_t *cpu_affinity;

    static void* main(void * arg);
    int stop(int seconds);
    int proc_server_exit();
};

class server_group_t
{
public:
    server_group_t();
    ~server_group_t();

    int stop_flag;
    std::string group_name;
    ServerGroup conf_data;
    int thread_num;
    std::vector<cpu_set_t> cpu_affinitys;
    std::vector<server_worker_t *> m_work_pool;

    static server_group_t * build(ServerGroup const &conf);

    int start();
    int stop(int seconds);
    rpc_pb_server_t *build_rpc_server(RpcServer const & conf);
};

class ServerContainer
{
public:
    std::vector<server_group_t *> server_groups;
    int start();
    int stop(int seconds);
};

class ServerBuilder
{
public:
    static ServerContainer *build(ServerConf const &conf); 
};

}
