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
#include "core/incl/auto_var.h"

using namespace conet_rpc_pb;

namespace conet
{

static std::map<std::string , std::map<std::string, rpc_pb_cmd_t> > *g_server_cmd_maps=NULL;

static std::map<std::string , std::map<std::string, http_cmd_t> > *g_server_http_cmd_maps=NULL;

static 
void clear_g_server_maps(void)
{
    delete g_server_cmd_maps;
    delete g_server_http_cmd_maps;
    g_server_cmd_maps = NULL;
    g_server_http_cmd_maps = NULL;
}

int registry_cmd(std::string const & server_name, std::string const & name,  rpc_pb_callback proc, http_callback hproc, void *arg)
{
    if (NULL == g_server_cmd_maps) {
        g_server_cmd_maps = new typeof(*g_server_cmd_maps);
        g_server_http_cmd_maps = new typeof(*g_server_http_cmd_maps);
        atexit(clear_g_server_maps);
    }
    std::map<std::string, rpc_pb_cmd_t> & maps = (*g_server_cmd_maps)[server_name];
    rpc_pb_cmd_t item; 
    item.name = name;
    item.proc = proc;
    item.arg = arg;
    maps.insert(std::make_pair(name, item));


    { // registry http cmd
        std::map<std::string, http_cmd_t> & maps = (*g_server_http_cmd_maps)[server_name];
        http_cmd_t item; 
        item.name = name;
        item.proc = hproc;
        item.arg = arg;
        maps.insert(std::make_pair(std::string("/rpc/") + name, item));
    }
    return 0;
}

int registry_cmd(rpc_pb_server_t *server, std::string const & name,  rpc_pb_callback proc, http_callback hproc, void *arg )
{

    if (server->cmd_maps.find(name) != server->cmd_maps.end()) {
        return -1;
    }
    rpc_pb_cmd_t item; 
    item.name = name;
    item.proc = proc;
    item.arg = arg;
    server->cmd_maps.insert(std::make_pair(name, item));

    if (server->http_server) {
        AUTO_VAR(&maps, =, server->http_server->cmd_maps);
        http_cmd_t item; 
        item.name = name;
        item.proc = hproc;
        item.arg = arg;
        maps.insert(std::make_pair(std::string("/rpc/")+name, item));
    }

    return 0;
}


int unregistry_cmd(rpc_pb_server_t *server, std::string const &name)
{
    AUTO_VAR(it, =, server->cmd_maps.find(name));
    if (it == server->cmd_maps.end()) {
        return -1;
    }
    server->cmd_maps.erase(it);
    server->http_server->cmd_maps.erase(name);
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
    server->http_server->cmd_maps = (*g_server_http_cmd_maps)[server->server_name];
    return server->cmd_maps.size();
}

static int proc_rpc_pb(conn_info_t *conn);

int init_server(
        rpc_pb_server_t *self, 
        std::string const &server_name, 
        char const *ip,
        int port,
        bool use_global_cmd
    )
{
    self->server_name =server_name;
    server_t *server_base = new server_t();
    int ret = 0;
    ret = init_server(server_base, ip, port);
    if (ret) {
        delete server_base;
        LOG(ERROR)<<"init server_baser in rpc server failed, [ret:"<<ret<<"]";
        return -1;
    }
    self->server = server_base; 

    /*
    server_t *server_base2 = new server_t();
    ret = init_server(server_base2, ip, port+10000);
    if (ret) {
        delete server_base2;
        LOG(ERROR)<<"init server_baser in http server failed, [ret:"<<ret<<"]";
        return -1;
    }
    */

    http_server_t *http_server = new http_server_t();
    http_server->server = server_base; 
    http_server->extend = self;
    self->http_server = http_server;
    if (use_global_cmd) {
        get_global_server_cmd(self);
    }
    return 0;
}

int start_server(rpc_pb_server_t *server)
{
    server->server->extend = server;
    server->server->proc = proc_rpc_pb;
    if (server->cmd_maps.empty() ) {
        return -1;
    }
    int ret =  0;
    ret = start_server(server->server);
    //start_server(server->http_server);
    return ret;
}


static int proc_rpc_pb(conn_info_t *conn)
{
    server_t * server_base= conn->server; 
    rpc_pb_server_t * server = (rpc_pb_server_t *) server_base->extend;
    int max_size = server_base->max_packet_size;
    int ret = 0;
    int fd = conn->fd;

    rpc_pb_ctx_t ctx;

    CmdBase cmd_base;

    CmdBaseResp resp_base;

    ctx.server = server;
    ctx.conn_info = conn;
    ctx.req = &cmd_base;
    ctx.to_close = 0;

    PacketStream stream;
    stream.init(fd, max_size);
    std::vector<char> out_buf;
    uint32_t out_len = max_size;
    std::string errmsg;
    std::string resp;
    do
    {

        struct pollfd pf = {
            fd: fd,
            events: POLLIN | POLLERR | POLLHUP
        };

        poll( &pf, 1, 1000);

        if (pf.revents & POLLERR) {
            break;
        }

        if (!(pf.revents & POLLIN)) {
            continue;
        }

        char * data = NULL;
        int packet_len = 0;

        ret = stream.read_packet(&data, &packet_len, 100, 1); 
        if (ret == 0) {
            break;
        }

        if (ret <0) {
            if (ret == PacketStream::HTTP_PROTOCOL_DATA) {
                conn->extend = &stream;
                http_server_proc2(conn, server_base, server->http_server);
                break;
            } else {
                LOG(ERROR)<<"read 4 byte pack failed, fd:"<<fd<<", ret:"<<ret;
                break;
            }
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
            ret = send_pb_obj(fd, cmd_base, &out_buf);
            if (ret <= 0) {
                LOG(ERROR)<<"send resp failed!, fd:"<<fd<<", ret:"<<ret;
            }
            break;
        }

        std::string * req = cmd_base.mutable_body();

        resp.resize(0);
        errmsg.resize(0);

        int retcode = cmd->proc(cmd->arg, &ctx, req, &resp, &errmsg);

        cmd_base.set_type(CmdBase::RESPONSE_TYPE);
        cmd_base.set_ret(retcode);
        if (!errmsg.empty()) cmd_base.set_errmsg(errmsg);
        if (!resp.empty()) cmd_base.set_body(resp);

        ret = send_pb_obj(fd, cmd_base, &out_buf);
        if (ret <=0) {
            // send data failed;
            LOG(ERROR)<<"send resp failed!, fd:"<<fd<<", ret:"<<ret;
            break; 
        }
        if (ctx.to_close) {
            break;
        }
    } while(1);
    return 0;
}

}
