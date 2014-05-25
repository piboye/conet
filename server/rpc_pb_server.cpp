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
#include "core/incl/log.h"
#include "core/incl/net_tool.h"

#define AUTO_VAR(a, op, val) typeof(val) a op val

using namespace conet_rpc_pb;

namespace conet
{

static std::map<std::string , std::map<std::string, rpc_pb_cmd_t> > g_server_cmd_maps;

int registry_cmd(std::string const & server_name, std::string const & name,  rpc_pb_callback proc, void *arg )
{
    std::map<std::string, rpc_pb_cmd_t> & maps = g_server_cmd_maps[server_name];
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
    server->cmd_maps = g_server_cmd_maps[server->server_name];
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
    int size = server_base->max_packet_size;
    //char * buff = CO_ALLOC_ARRAY(char, size);
    int ret = 0;
    int fd = conn->fd;
    set_none_block(fd, false);

    rpc_pb_ctx_t ctx;

    CmdBase cmd_base;

    CmdBaseResp resp_base;

    ctx.server = server;
    ctx.conn_info = conn;
    ctx.req = &cmd_base;
    do
    {
        std::string data;

        ret = read_one_pack(fd,  &data,  1000, size, NULL);
        if (ret == 0) {
            break;
        }

        if (ret <0) {
            CONET_LOG(ERROR, " read 4 byte pack failed, fd:%d, ret:%d", fd, ret);
            break;
        }

        if (!cmd_base.ParseFromString(data)) {
            // parse cmd base head failed;
            CONET_LOG(ERROR, " parse cmd base failed, fd:%d, ret:%d", fd, ret);
            break;
        }

        if (cmd_base.type() != CmdBase::REQUEST_TYPE) {
            CONET_LOG(ERROR, "request type[%d] error, require REQUEST_TYPE:%d",
                    (int ) cmd_base.type(), (int) CmdBase::REQUEST_TYPE);
            break;
        }

        if (cmd_base.server_name() != server->server_name) {
            CONET_LOG(ERROR, "request server name[%s] nomatch cur server[%s]",
                    cmd_base.server_name().c_str(), server->server_name.c_str());
            break;

        }

        std::string cmd_name = cmd_base.cmd_name();
        rpc_pb_cmd_t * cmd = get_rpc_pb_cmd(server, cmd_name);
        if (NULL == cmd) {
            // not unsuppend cmd;
            CONET_LOG(ERROR, "unsuppend cmd, server:%s, cmd:%s",
                    server->server_name.c_str(), cmd_name.c_str());

            cmd_base.set_ret(CmdBase::ERR_UNSUPPORED_CMD);
            cmd_base.set_errmsg("unsuppored cmd");
            ret = send_data_pack(fd, cmd_base.SerializeAsString());
            if (ret <= 0) {
                CONET_LOG(ERROR, "send resp failed!, fd:%d, ret:%d", fd, ret);
                break;
            }
            break;
        }

        std::string req = cmd_base.body();
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
            CONET_LOG(ERROR, "send resp failed!, fd:%d, ret:%d", fd, ret);
            break; 
        }
    } while(1);
    return 0;
}

int rpc_pb_call_impl(char const *ip, int port, 
        std::string const &server_name,
        std::string const &cmd_name,
        std::string const &req, std::string *resp, std::string *errmsg)
{
    int fd = 0;
    fd = connect_to(ip, port);
    if (fd <0) return -3;
    int ret = 0;

    conet_rpc_pb::CmdBase req_base, resp_base;
    req_base.set_server_name(server_name);
    req_base.set_cmd_name(cmd_name);
    req_base.set_seq_id(1);
    req_base.set_type(conet_rpc_pb::CmdBase::REQUEST_TYPE);
    req_base.set_body(req);

    ret = send_data_pack(fd, req_base.SerializeAsString());
    if (ret <=0) {
        close(fd);
        return -4;
    }
    std::string data;
    ret = read_one_pack(fd, &data);
    close(fd);
    if (ret <=0) {
        return -5;
    }
    if (!resp_base.ParseFromString(data)) {
        return -6;
    }
    ret = resp_base.ret();
    if (ret) {
        if (errmsg) {
            *errmsg = resp_base.errmsg();
        }
        return ret;
    }

    if (resp) {
        *resp = resp_base.body();
    }
    return ret;
}

}
