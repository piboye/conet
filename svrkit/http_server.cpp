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
#include "core/incl/auto_var.h"
#include <stdarg.h>

namespace conet
{

void response_to(http_response_t *resp, int http_code, std::string const &body) 
{
    resp->http_code = http_code;
    resp->body = body; 
}

void response_format(http_response_t *resp, int http_code, char const *fmt, ...) 
{
    resp->http_code = http_code;
    char buf[100];
    size_t len = sizeof(buf);
    size_t nlen = 0;
    char *p = buf;
    va_list ap;
    va_list bak_arg;
    va_start(ap, fmt);
    va_copy(bak_arg, ap);
    nlen = vsnprintf(p, len, fmt, ap);
    if (nlen > len) {
        p = (char *)malloc(nlen+1);
        len = nlen;
        nlen = vsnprintf(p, len, fmt, bak_arg);
        va_end(bak_arg);
    }
    va_end(ap);

    resp->body.assign(p, nlen);
    if (p != buf) {
        free(p);
    }
}


void init_http_response(http_response_t *self)
{
        self->http_code = 200;
        self->keepalive = 0;
}

int output_response(http_response_t *resp, int fd)
{
    std::string out;
    char buf[100];
    int len = snprintf(buf, sizeof(buf), "HTTP/1.0 %d\r\n", resp->http_code);    
    out.assign(buf, len);
    for (int i=0, n = (int)resp->headers.size(); i<n; ++i)
    {
        out+=resp->headers[i];
    }

    if (resp->keepalive) {
        out.append("Connection: Keep-Alive\r\n");
    } else {
        out.append("Connection: close\r\n");
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

int http_server_main(conn_info_t *conn, http_request_t *req,
        server_t *server_base,
        http_server_t *http_server
        )
{
    std::string path;
    ref_str_to(&req->path, &path);
    int ret = 0;

    http_response_t resp;
    init_http_response(&resp);

    http_ctx_t ctx;
    if (req->connection == CONNECTION_KEEPALIVE && http_server->enable_keepalive)  {
        resp.keepalive = 1;
    }
    ctx.to_close = 0;
    ctx.server = http_server;
    ctx.conn_info = conn;

    http_cmd_t *cmd = get_http_cmd(http_server, path);
    if (cmd == NULL) {
        LOG(ERROR)<<"no found path cmd, [path:"<<path<<"]";
        ctx.to_close = 1;
        resp.keepalive = 0;
        response_to(&resp, 404, "");
    } else {
        ret = cmd->proc(cmd->arg, &ctx, req, &resp);
        if (ret) {
            ctx.to_close = 1;
        }
    }

    ret = output_response(&resp, conn->fd);
    if (ret <=0) {
        LOG(ERROR)<<"send response failed [ret="<<ret<<"]";
        return -1;
    }
    if (resp.keepalive == 0 || ctx.to_close) 
    {
        return 1;
    }
    return 0;
}

int http_server_proc(conn_info_t *conn) 
{

    server_t *base_server = conn->server;
    http_server_t *http_server = (http_server_t *) conn->extend;
    return http_server_proc2(conn, base_server, http_server);
}

int http_server_proc2(conn_info_t *conn, 
        server_t *base_server, http_server_t *http_server) 
{

    int fd  = conn->fd;

    http_request_t req;
    http_request_init(&req);

    int len = 4*1024;
    char *buf = NULL;
    ssize_t nparsed = 0;
    ssize_t end =  0;
    int ret = 0;
    ssize_t recved = 0;

    PacketStream *stream = (PacketStream *) conn->extend;
    if (NULL == stream) {
        buf = (char *)malloc(len);
    } else  {
        recved = stream->prev_pos;
        if (stream->max_size < len) {
            buf = (char *)malloc(len);
            memcpy(buf, stream->buff, recved);
        }  else {
            buf = stream->buff;    
            len = stream->max_size;
            stream->buff = NULL;
        }
        conn->extend = NULL;
    }

    uint64_t cnt = 0; 
    do 
    {

        if (cnt > 0) {
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
        }
        ++cnt;

        end = recved + nparsed;

        nparsed += http_request_parse(&req, buf, end, nparsed);
        ret = http_request_finish(&req); 
        switch(ret)
        {
            case 1: // finished;
                {
                    ret = http_server_main(conn, &req, base_server, http_server);
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


    } while(ret == 0 && base_server->to_stop == 0);

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

int stop_server(http_server_t *server, int wait)
{
    int ret = 0;
    ret = stop_server(server->server, wait);
    return ret;
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
