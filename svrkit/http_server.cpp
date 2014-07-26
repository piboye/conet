/*
 * =====================================================================================
 *
 *       Filename:  http_server.cpp
 *
 *    Description:  
 *
 *        Version:  1.0
 *        Created:  2014年07月23日 17时23分11秒
 *       Revision:  none
 *       Compiler:  gcc
 *
 *         Author:  piboyeliu
 *   Organization:  
 *
 * =====================================================================================
 */

#include <stdlib.h>
#include "http_parser.h"
#include "core/incl/net_tool.h"
#include "server_base.h"
#include "http_server.h"
#include "thirdparty/glog/logging.h"

#define AUTO_VAR(a, op, val) typeof(val) a op val

namespace conet
{


void init_http_response(http_response_t *self)
{
        self->http_code = 200;
        self->keepalive = 0;
}

int output_response(http_response_t *resp, int fd)
{
    std::string out;
    char buf[100];
    int len = snprintf(buf, sizeof(buf), "HTTP/1.1 %d\r\n", resp->http_code);    
    out.assign(buf, len);
    for (int i=0, n = (int)resp->headers.size(); i<n; ++i)
    {
        out+=resp->headers[i];
    }

    if (resp->keepalive) {
        out.append("Connection: Keep-Alive\r\n");
    }

    len = resp->body.size();
    len = snprintf(buf, sizeof(buf), "Content-Length: %d\r\n", len);
    out.append(buf, len);

    out.append("\r\n");
    out.append(resp->body);
    return send_data(fd, out.c_str(), out.size(), 1000); 
}


http_cmd_t * get_http_cmd(http_server_t *server, std::string const &name)
{
    AUTO_VAR(it, =, server->cmd_maps.find(name));
    if (it == server->cmd_maps.end()) {
        return NULL;
    }
    return &it->second;
}

int http_server_main(conn_info_t *conn, http_request_t *req)
{
    std::string path;
    ref_str_to(&req->path, &path);

    server_t * server_base= conn->server; 
    http_server_t * http_server = (http_server_t *) server_base->extend;

    http_cmd_t *cmd = get_http_cmd(http_server, path);
    if (cmd == NULL) {
        return 0;
    }
    
    http_response_t resp;
    init_http_response(&resp);

    http_ctx_t ctx;
    ctx.to_close = 0;
    ctx.server = http_server;
    ctx.conn_info = conn;
    int ret = cmd->proc(cmd->arg, &ctx, req, &resp);
    if (ret) {
        return ret;
    }

    ret = output_response(&resp, conn->fd);
    if (ret <=0) {
        LOG(ERROR)<<"send response failed [ret="<<ret<<"]";
        return -1;
    }
    return 0;
}

int http_server_proc(conn_info_t *conn) 
{

    int fd  = conn->fd;

    http_request_t req;
    http_request_init(&req);

    int len = 4*1024;
    char *buf = (char *)malloc(len);

    ssize_t nparsed = 0;
    ssize_t end =  0;
    int ret = 0;
    do 
    {
        ssize_t recved;

        recved = recv(fd, buf+nparsed, len-nparsed, 0);
        if (recved == 0) {
            if (nparsed == 0) {
                ret = 0;
                break;
            }
            ret = -2;
            LOG(ERROR)<<"recv failed";
            break;
        }
        if (recved < 0) {
            LOG(ERROR)<<"recv failed [ret="<<ret<<"]";
            ret = -2;
            break;
        }
        end = recved + nparsed;

        nparsed += http_request_parse(&req, buf, end, nparsed);
        ret = http_request_finish(&req); 
        switch(ret)
        {
            case 1: // finished;
                {
                    ret = http_server_main(conn, &req);
                    nparsed = 0;
                    break;
                }
                break;
            case 0:
                ret = 0;
                break;
            default:
                ret = -1;
                break;
        }

        ret == 0;
    } while(ret == 0);

    free(buf);

    return 0;
}

int start_server(http_server_t *server)
{
    server->server->extend = server;
    server->server->proc = http_server_proc;
    if (server->cmd_maps.empty() ) {
        return -1;
    }
    return start_server(server->server);
}

int registry_cmd(http_server_t *server, std::string const & name,  http_callback proc, void *arg )
{
    if (server->cmd_maps.find(name) != server->cmd_maps.end()) {
        return -1;
    }
    http_cmd_t item; 
    item.name = name;
    item.proc = proc;
    item.arg = arg;
    server->cmd_maps.insert(std::make_pair(name, item));
    return 0;
}
}
