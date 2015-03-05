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
 *         Author:  piboye
 *   Organization:  
 *
 * =====================================================================================
 */

#include <stdlib.h>
#include <stdarg.h>

#include "http_server.h"
#include "thirdparty/glog/logging.h"

#include "base/auto_var.h"
#include "base/http_parser.h"
#include "base/net_tool.h"
#include "base/ptr_cast.h"

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


http_cmd_t * http_server_t::get_http_cmd(std::string const &name)
{
    AUTO_VAR(it, =, this->cmd_maps.find(name));
    if (it == this->cmd_maps.end()) {
        return NULL;
    }
    return it->second;
}

int http_server_main(conn_info_t *conn, http_request_t *req,
        tcp_server_t *server_base,
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

    http_cmd_t *cmd = http_server->get_http_cmd(path);
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

int http_server_t::conn_proc(conn_info_t *conn) 
{
    tcp_server_t *base_server = this->tcp_server;

    int fd  = conn->fd;

    http_request_t req;

    int len = 4*1024;
    char *buf = NULL;
    ssize_t nparsed = 0;
    ssize_t end =  0;
    int ret = 0;
    ssize_t recved = 0;
    int malloc_buff = 0;

    PacketStream *stream = (PacketStream *) conn->extend;
    if (NULL == stream) {
        malloc_buff = 1;
        buf = (char *)malloc(len);
    } else  {
        recved = stream->prev_pos;
        if (stream->max_size < len) {
            malloc_buff = 1;
            buf = (char *)malloc(len);
            memcpy(buf, stream->buff, recved);
        }  else {
            buf = stream->buff;    
            len = stream->max_size;
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
                LOG(INFO)<<"recv failed";
                break;
            }
            if (recved < 0) {
                LOG(INFO)<<"recv failed [ret="<<ret<<"]";
                ret = -2;
                break;
            }
        }
        ++cnt;

        end = recved + nparsed;

        //初始化 http req
        http_request_init(&req);
        nparsed += http_request_parse(&req, buf, end, nparsed);
        ret = http_request_finish(&req); 
        switch(ret)
        {
            case 1: // finished;
                {
                    ret = http_server_main(conn, &req, base_server, this);
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


    if (malloc_buff == 1) free(buf);

    return ret;
}

int http_server_t::init(tcp_server_t *tcp_server)
{
    this->tcp_server = tcp_server;
    tcp_server->extend = this;
    tcp_server->set_conn_cb(ptr_cast<tcp_server_t::conn_proc_cb_t>(&http_server_t::conn_proc), this);
    return 0;
}

int http_server_t::start()
{
    if (this->cmd_maps.empty() ) {
        return -1;
    }
    return this->tcp_server->start();
}

int http_server_t::stop(int wait)
{
    int ret = 0;
    ret = tcp_server->stop(wait);
    return ret;
}

int http_server_t::registry_cmd(std::string const & name,  
        http_callback proc, 
        void *arg, 
        void (*free_arg)(void *arg)
        )
{
    if (this->cmd_maps.find(name) != this->cmd_maps.end()) {
        return -1;
    }
    http_cmd_t *item = new http_cmd_t; 
    item->name = name;
    item->proc = proc;
    item->arg = arg;
    item->free_fn = free_arg;
    this->cmd_maps.insert(std::make_pair(name, item));
    return 0;
}

http_server_t::http_server_t()
{
    tcp_server = NULL;
    extend = NULL;
    enable_keepalive = 0;
}

http_server_t::~http_server_t()
{
    for(typeof(cmd_maps.begin()) it=cmd_maps.begin(), 
                iend = cmd_maps.end();
                it!=iend; ++it)
    {
        delete it->second;
    }
}

}
