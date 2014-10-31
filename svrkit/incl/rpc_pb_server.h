/*
 * =====================================================================================
 *
 *       Filename:  pb_rpbc_server.h
 *
 *    Description:  
 *
 *        Version:  1.0
 *        Created:  2014年05月25日 03时18分43秒
 *       Revision:  none
 *       Compiler:  gcc
 *
 *         Author:  piboye
 *   Organization:  
 *
 * =====================================================================================
 */
#ifndef __PB_RPC_SERVER_H_INC__
#define __PB_RPC_SERVER_H_INC__
#include "server_base.h"
#include <string>
#include <map>

#include "svrkit/rpc_base_pb.pb.h"
#include "http_server.h"
#include "google/protobuf/descriptor.h"
#include "google/protobuf/message.h"
#include "base/incl/obj_pool.h"
#include "base/incl/str_map.h"
#include "base/incl/query_string.h"
#include "base/incl/pb2json.h"
#include "base/incl/pb_obj_pool.h"

namespace conet
{

struct rpc_pb_server_t;
struct rpc_pb_ctx_t
{
    int to_close; // close connection when set 1
    conn_info_t * conn_info;
    rpc_pb_server_t *server;
    conet_rpc_pb::CmdBase *req;
    void * arg;
};

typedef int (*rpc_pb_callback)(void *, rpc_pb_ctx_t *ctx, \
        google::protobuf::Message * req, google::protobuf::Message *resp,  \
        std::string * errmsg);

template <typename T1, typename R1, typename R2>
R1 get_request_type_from_rpc_pb_func( int (*fun2) (T1 *arg, rpc_pb_ctx_t *ctx, R1 *req, R2 *resp, std::string *errmsg));

template <typename T1, typename R1, typename R2>
R2 get_response_type_from_rpc_pb_func( int (*fun2) (T1 *arg, rpc_pb_ctx_t *ctx, R1 *req, R2*resp, std::string *errmsg));


struct rpc_stat_base_t 
{
   int cnt;
   int max_cost;
   int min_cost;
   int64_t total_cost;
   rpc_stat_base_t()
   {
        cnt = 0;
        max_cost = 0;
        min_cost = 0;
        total_cost = 0;
   }
};

struct cmd_class_obj_mgr_base_t
{
   virtual void * alloc() = 0;
   virtual void free(void *) = 0;

   virtual ~cmd_class_obj_mgr_base_t() = 0;
   virtual cmd_class_obj_mgr_base_t *clone() = 0;
};

struct rpc_pb_cmd_t
{
   rpc_pb_cmd_t()
   {
       obj_mgr =NULL;
   }

   ~rpc_pb_cmd_t()
   {
       if (req_msg) delete req_msg;
       if (rsp_msg) delete rsp_msg;
       if (obj_mgr) {
          delete obj_mgr; 
       }
   }

   void init(std::string const &method_name, google::protobuf::Message *req, google::protobuf::Message *rsp)
   {
        this->method_name = method_name;
        this->cmd_map_node.init(ref_str(this->method_name));
        this->req_msg = req;
        this->rsp_msg = rsp;
        this->req_pool.init(this->req_msg, 0);
        this->rsp_pool.init(this->rsp_msg, 0);
   }

   rpc_pb_cmd_t * clone() const
   {
        rpc_pb_cmd_t * n = new rpc_pb_cmd_t();
        n->init(this->method_name, this->req_msg, this->rsp_msg); 
        n->arg = this->arg;
        n->proc = this->proc;
        if (n->obj_mgr) 
            n->obj_mgr = this->obj_mgr->clone();
        return n;
   }

   rpc_pb_callback proc;

   void *arg; 
   
   std::string method_name;
   StrMap::node_type cmd_map_node;

   google::protobuf::Message * req_msg;
   google::protobuf::Message * rsp_msg;
   PbObjPool req_pool; 
   PbObjPool rsp_pool; 

   cmd_class_obj_mgr_base_t *obj_mgr;
   // stat data
   // [0] => 5s [1] => 1 minute [2] => 5 minute [3] => 1 hour [4] => 1 day
   rpc_stat_base_t success_stat[5]; 
   rpc_stat_base_t failed_stat[5];
   
   std::map<int, int64_t> error_ret_stat; // error return code summory

};


struct rpc_pb_server_t
{
    struct server_t * server;
    http_server_t *http_server;
    StrMap cmd_maps;
    int async_flag; //default 0
    rpc_pb_server_t();
};

int get_global_server_cmd(rpc_pb_server_t * server);

int registry_cmd(rpc_pb_cmd_t  *cmd);

rpc_pb_cmd_t * get_rpc_pb_cmd(rpc_pb_server_t *server, std::string const &name);



int init_server(
        rpc_pb_server_t *self, 
        char const *ip,
        int port,
        char const *http_ip=NULL,
        int http_port=0
    );

int start_server(rpc_pb_server_t *server);

int stop_server(rpc_pb_server_t *server, int wait=0);

google::protobuf::Message * pb_obj_new(google::protobuf::Message *msg);

template <typename T1, typename R1, typename R2>
int registry_rpc_pb_cmd(std::string const &method_name,
        int (*func) (T1 *, rpc_pb_ctx_t *ctx, R1 *req, R2*rsp, std::string *errmsg), void *arg)
{
    int ret = 0;
    rpc_pb_cmd_t * cmd = new rpc_pb_cmd_t(); 

    cmd->init(method_name, new typeof(R1), new typeof(R2));

    cmd->proc = (rpc_pb_callback)(func); 
    cmd->arg = (void *)arg; 
    ret = conet::registry_cmd(cmd); 
    if (ret)  {
       delete cmd;
    }
    return 1; 
}

template <typename T1, typename R1, typename R2>
int registry_rpc_pb_cmd(std::string const &method_name,
        int (T1::*func) (rpc_pb_ctx_t *ctx, R1 *req, R2*rsp, std::string *errmsg), T1* arg)
{
    int ret = 0;
    rpc_pb_cmd_t * cmd = new rpc_pb_cmd_t(); 
    cmd->init(method_name, new typeof(R1), new typeof(R2));

    //cmd->proc = (rpc_pb_callback)(func);  // 这会引起告警， 换成下面的方式就不会, i hate c++ !!!
    memcpy(&(cmd->proc), &(func), sizeof(void *));

    cmd->arg = (void *)arg; 
    ret = conet::registry_cmd(cmd); 
    if (ret) {
        delete cmd;
    }
    return 1; 
}

template <typename T1, typename R1, typename R2>
int registry_rpc_pb_cmd(std::string const &method_name,
        int (T1::*func) (rpc_pb_ctx_t *ctx, R1 *req, R2*rsp, std::string *errmsg), 
        cmd_class_obj_mgr_base_t *obj_mgr)
{
    int ret = 0;
    rpc_pb_cmd_t * cmd = new rpc_pb_cmd_t(); 
    cmd->init(method_name, new typeof(R1), new typeof(R2));

    //cmd->proc = (rpc_pb_callback)(func);  // 这会引起告警， 换成下面的方式就不会, i hate c++ !!!
    memcpy(&(cmd->proc), &(func), sizeof(void *));

    cmd->arg = (void *)NULL; 
    cmd->obj_mgr = obj_mgr;
    ret = conet::registry_cmd(cmd); 
    if (ret) {
        delete cmd;
    }
    return 1; 
}



#define CONET_MACRO_CONCAT_IMPL(a, b) a##b
#define CONET_MACRO_CONCAT(a, b) CONET_MACRO_CONCAT_IMPL(a,b)

#define REGISTRY_RPC_PB_FUNC(method_name, func, arg) \
    static int CONET_MACRO_CONCAT(g_rpc_pb_registry_cmd_, __LINE__) = conet::registry_rpc_pb_cmd(method_name, func, arg)


template <typename T>
class CmdClassObjMgrHelp
    :public cmd_class_obj_mgr_base_t
{
public:
    typedef T class_name;

    void * alloc() 
    {
        return new class_name(); 
    }

    cmd_class_obj_mgr_base_t * clone()
    {
        return new typeof(*this);
    }

    void free(void *arg)
    {
         T * self = (T *)(arg);
         delete  self;
    }

    virtual ~CmdClassObjMgrHelp()
    {

    }
};

#define REGISTRY_RPC_PB_CLASS_MEHTORD(method_name, Class, func) \
    static int CONET_MACRO_CONCAT(g_rpc_pb_registry_cmd_,__LINE__) = \
        conet::registry_rpc_pb_cmd(method_name, &Class::func, new conet::CmdClassObjMgrHelp<Class>())

}
#endif /* end of include guard */ 
