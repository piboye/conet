/*
 * =====================================================================================
 *
 *       Filename:  rpc_pb_server.cpp
 *
 *    Description:  
 *
 *        Version:  1.0
 *        Created:  2014年05月25日 07时48分45秒
 *       Revision:  none
 *       Compiler:  gcc
 *
 *         Author:  YOUR NAME (), 
 *   Organization:  
 *
 * =====================================================================================
 */
#include <stdlib.h>
#include "rpc_pb_server.h"
#include "net_tool.h"

#define AUTO_VAR(a, op, val) typeof(val) a op val

using namespace conet_rpc_pb;

namespace conet
{

static std::map<std::string , std::map<std::string, rpc_pb_cmd_t> > *g_server_cmd_maps=NULL;

static 
void clear_g_server_maps(void)
{
    delete g_server_cmd_maps;
    g_server_cmd_maps = NULL;
}

int registry_cmd(std::string const & server_name, std::string const & name,  rpc_pb_callback proc, void *arg )
{
    if (NULL == g_server_cmd_maps) {
        g_server_cmd_maps = new typeof(*g_server_cmd_maps);
        atexit(clear_g_server_maps);
    }
    std::map<std::string, rpc_pb_cmd_t> & maps = (*g_server_cmd_maps)[server_name];
    rpc_pb_cmd_t item; 
    item.name = name;
    item.proc = proc;
    item.arg = arg;
    maps.insert(std::make_pair(name, item));
    return 0;
}

int registry_cmd( rpc_pb_server_t *server, std::string const & name,  rpc_pb_callback proc, void *arg )
{

    if (server->cmd_maps.find(name) != server->cmd_maps.end()) {
        return -1;
    }
    rpc_pb_cmd_t item; 
    item.name = name;
    item.proc = proc;
    item.arg = arg;
    server->cmd_maps.insert(std::make_pair(name, item));
    return 0;
}


int unregistry_cmd(rpc_pb_server_t *server, std::string const &name)
{
    AUTO_VAR(it, =, server->cmd_maps.find(name));
    if (it == server->cmd_maps.end()) {
        return -1;
    }
    server->cmd_maps.erase(it);
    return 0;
}

rpc_pb_cmd_t * get_rpc_pb_cmd(rpc_pb_server_t *server, std::string const &name)
{
    AUTO_VAR(it, =, server->cmd_maps.find(name));
    if (it == server->cmd_maps.end()) {
        return NULL;
    }
    return &it->second;
}


int get_global_server_cmd(rpc_pb_server_t * server) 
{
    server->cmd_maps = (*g_server_cmd_maps)[server->server_name];
    return server->cmd_maps.size();
}

static int proc_rpc_pb(conn_info_t *conn);


int start_server(rpc_pb_server_t *server)
{
    server->server->extend = server;
    server->server->proc = proc_rpc_pb;
    if (server->cmd_maps.empty() ) {
        return -1;
    }
    return start_server(server->server);
}


static int proc_rpc_pb(conn_info_t *conn)
{
    server_t * server_base= conn->server; 
    rpc_pb_server_t * server = (rpc_pb_server_t *) server_base->extend;
    int max_size = server_base->max_packet_size;
    int ret = 0;
    int fd = conn->fd;
    //set_none_block(fd, false);

    rpc_pb_ctx_t ctx;

    CmdBase cmd_base;

    CmdBaseResp resp_base;

    ctx.server = server;
    ctx.conn_info = conn;
    ctx.req = &cmd_base;

    PacketStream stream;
    stream.init(fd, max_size);
    do
    {
        char * data = NULL;
        int packet_len = 0;

        ret = stream.read_packet(&data, &packet_len);
        if (ret == 0) {
            break;
        }

        if (ret <0) {
            LOG(ERROR)<<"read 4 byte pack failed, fd:"<<fd<<", ret:"<<ret;
            break;
        }

        if (!cmd_base.ParseFromArray(data, packet_len)) 
        {
            // parse cmd base head failed;
            LOG(ERROR)<<"parse cmd base failed, fd:"<<fd<<", ret:"<<ret;
            break;
        }
        

        if (cmd_base.type() != CmdBase::REQUEST_TYPE) {
            LOG(ERROR)<<"request type["<<cmd_base.type()<<"] error, require REQUEST_TYPE:"<<CmdBase::REQUEST_TYPE;
            break;
        }

        if (cmd_base.server_name() != server->server_name) {
            LOG(ERROR)<< "request server["<<cmd_base.server_name()<<"] nomatch the server["<<server->server_name<<"]";
            break;

        }

        std::string cmd_name = cmd_base.cmd_name();
        rpc_pb_cmd_t * cmd = get_rpc_pb_cmd(server, cmd_name);
        if (NULL == cmd) {
            // not unsuppend cmd;
            LOG(ERROR)<< "unsuppend cmd, server:"<<server->server_name
                <<"cmd:"<<cmd_name;

            cmd_base.set_ret(CmdBase::ERR_UNSUPPORED_CMD);
            cmd_base.set_errmsg("unsuppored cmd");
            ret = send_data_pack(fd, cmd_base.SerializeAsString());
            if (ret <= 0) {
                LOG(ERROR)<<"send resp failed!, fd:"<<fd<<", ret:"<<ret;
                break;
            }
            break;
        }

        std::string & req = cmd_base.body();
        std::string resp;
        std::string errmsg;

        int retcode = cmd->proc(cmd->arg, &ctx, &req, &resp, &errmsg);

        cmd_base.set_ret(retcode);
        if (!errmsg.empty()) cmd_base.set_errmsg(errmsg);
        if (!resp.empty()) cmd_base.set_body(resp);
        cmd_base.set_type(CmdBase::RESPONSE_TYPE);
        ret = send_data_pack(fd, cmd_base.SerializeAsString());
        if (ret <=0) {
            // send data failed;
            LOG(ERROR)<<"send resp failed!, fd:"<<fd<<", ret:"<<ret;
            break; 
        }
    } while(1);
    return 0;
}


}
