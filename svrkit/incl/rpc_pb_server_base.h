/*
 * =====================================================================================
 *
 *       Filename:  rpc_pb_server_base.h
 *
 *    Description:  
 *
 *        Version:  1.0
 *        Created:  2014年11月02日 05时47分50秒
 *       Revision:  none
 *       Compiler:  gcc
 *
 *         Author:  piboye
 *   Organization:  
 *
 * =====================================================================================
 */


#ifndef __PB_RPC_SERVER_BASE_H_INC__
#define __PB_RPC_SERVER_BASE_H_INC__
#include <string>

#include "svrkit/rpc_base_pb.pb.h"
#include "google/protobuf/message.h"
#include "base/incl/str_map.h"
#include "base/incl/int_map.h"
#include "base/incl/pb_obj_pool.h"
#include "conn_info.h"
#include "base/incl/fn_ptr_cast.h"

namespace conet
{

struct rpc_pb_ctx_t
{
    int to_close; // close connection when set 1
    conn_info_t * conn_info;
    conet_rpc_pb::CmdBase *req;
    void * arg;
    void *server;
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

   void init(
           uint64_t cmd_id,
           std::string const &method_name, 
           google::protobuf::Message *req, 
           google::protobuf::Message *rsp
           )
   {
        this->cmd_id = cmd_id;
        this->method_name = method_name;
        this->cmd_map_node.init(ref_str(this->method_name));
        if (cmd_id >0) {
            this->cmd_id_map_node.init(cmd_id);
        }
        this->req_msg = req;
        this->rsp_msg = rsp;
        this->req_pool.init(this->req_msg, 0);
        this->rsp_pool.init(this->rsp_msg, 0);
   }

   rpc_pb_cmd_t * clone() const
   {
        rpc_pb_cmd_t * n = new rpc_pb_cmd_t();
        google::protobuf::Message *req = this->req_msg; 
        google::protobuf::Message *rsp = this->rsp_msg; 

        if (req) {
            req = req->New();
        }
        if (rsp) {
            rsp = rsp->New();
        }

        n->init(this->cmd_id, this->method_name, req, rsp);
        n->arg = this->arg;
        n->proc = this->proc;
        if (n->obj_mgr) 
            n->obj_mgr = this->obj_mgr->clone();
        return n;
   }

   // 命令函数, 这个是具体的业务代码
   rpc_pb_callback proc;

   // rpc 命令的回调参数， 如果是每次需要新对象, 不使用这个参数
   void *arg; 
   
   std::string method_name;
   uint64_t cmd_id;

   // 命令的 Map 节点
   StrMap::node_type cmd_map_node;

   IntMap::node_type cmd_id_map_node;

   // 请求和响应 的 protobuf 对象， 
   google::protobuf::Message * req_msg;
   google::protobuf::Message * rsp_msg;

   // 请求和响应 的 protobuf 对象池
   PbObjPool req_pool; 
   PbObjPool rsp_pool; 

   cmd_class_obj_mgr_base_t *obj_mgr; //  每次请求， 要分配一个新对象， 这些对象的分配和释放的管理器

   // 统计数据
   // stat data
   // [0] => 5s [1] => 1 minute [2] => 5 minute [3] => 1 hour [4] => 1 day
   rpc_stat_base_t success_stat[5]; 
   rpc_stat_base_t failed_stat[5];
   
   //错误码统计
   std::map<int, int64_t> error_ret_stat; // error return code summory

private:
   //disable copy and assignement, please use clone method
   rpc_pb_cmd_t(rpc_pb_cmd_t const &v);
   rpc_pb_cmd_t & operator=(rpc_pb_cmd_t const &v);
};

int global_registry_cmd(rpc_pb_cmd_t  *cmd);

template <typename T>
inline 
google::protobuf::Message *new_rpc_req_rsp_obj_help()
{
    return new T(); 
}

template <>
inline 
google::protobuf::Message *new_rpc_req_rsp_obj_help<void>()
{
    return NULL; 
}

template <typename T1, typename R1, typename R2>
int registry_rpc_pb_cmd(uint64_t cmd_id, std::string const &method_name, 
        int (*func) (T1 *, rpc_pb_ctx_t *ctx, R1 *req, R2*rsp, std::string *errmsg), void *arg)
{
    int ret = 0;
    rpc_pb_cmd_t * cmd = new rpc_pb_cmd_t(); 

    cmd->init(cmd_id, method_name, new_rpc_req_rsp_obj_help<typeof(R1)>(), new_rpc_req_rsp_obj_help<typeof(R2)>());

    cmd->proc = conet::fn_ptr_cast<rpc_pb_callback>(func); 
    cmd->arg = (void *)arg; 
    ret = conet::global_registry_cmd(cmd); 
    if (ret)  {
       delete cmd;
    }
    return 1; 
}

template <typename T1, typename R1, typename R2>
int registry_rpc_pb_cmd(uint64_t cmd_id, std::string const &method_name, 
        int (T1::*func) (rpc_pb_ctx_t *ctx, R1 *req, R2 *rsp, std::string *errmsg), T1* arg)
{
    int ret = 0;
    rpc_pb_cmd_t * cmd = new rpc_pb_cmd_t(); 
    cmd->init(cmd_id, method_name, new_rpc_req_rsp_obj_help<typeof(R1)>(), new_rpc_req_rsp_obj_help<typeof(R2)>());

    cmd->proc = conet::fn_ptr_cast<rpc_pb_callback>(func);  

    cmd->arg = (void *)arg; 
    ret = conet::global_registry_cmd(cmd); 
    if (ret) {
        delete cmd;
    }
    return 1; 
}

template <typename T1, typename R1, typename R2>
int registry_rpc_pb_cmd(uint64_t cmd_id, std::string const &method_name,
        int (T1::*func) (rpc_pb_ctx_t *ctx, R1 *req, R2*rsp, std::string *errmsg), 
        cmd_class_obj_mgr_base_t *obj_mgr)
{
    int ret = 0;
    rpc_pb_cmd_t * cmd = new rpc_pb_cmd_t(); 
    cmd->init(cmd_id, method_name, new_rpc_req_rsp_obj_help<typeof(R1)>(), new_rpc_req_rsp_obj_help<typeof(R2)>());

    cmd->proc = conet::fn_ptr_cast<rpc_pb_callback>(func);  

    cmd->arg = (void *)NULL; 
    cmd->obj_mgr = obj_mgr;
    ret = conet::global_registry_cmd(cmd);
    if (ret) {
        delete cmd;
    }
    return 1; 
}


#define CONET_MACRO_CONCAT_IMPL(a, b) a##b
#define CONET_MACRO_CONCAT(a, b) CONET_MACRO_CONCAT_IMPL(a,b)

#define REGISTRY_RPC_PB_FUNC(cmd_id, method_name, func, arg) \
    static int CONET_MACRO_CONCAT(g_rpc_pb_registry_cmd_, __LINE__) = conet::registry_rpc_pb_cmd(cmd_id, method_name, func, arg)


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

#define REGISTRY_RPC_PB_CLASS_MEHTORD(cmd_id, method_name, Class, func) \
    static int CONET_MACRO_CONCAT(g_rpc_pb_registry_cmd_,__LINE__) = \
        conet::registry_rpc_pb_cmd(cmd_id, method_name, &Class::func, new conet::CmdClassObjMgrHelp<Class>())

}


#endif /* end of include guard */ 
