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
#include "base/plog.h"

#include "base/auto_var.h"
#include "base/http_parser.h"
#include "base/net_tool.h"
#include "base/ptr_cast.h"
#include <openssl/sha.h>
#include "base/base64.h"
#include "base/string_builder.h"
#include <endian.h>
#include <sys/uio.h>

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
    char buf[1024];
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
    va_end(bak_arg);
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
        self->data =  NULL;
        self->data_len = 0;
}

static std::string const s_http_200_ok_keepalive = "HTTP/1.1 200\r\nConnection: keep-alive\r\n";
static std::string const s_http_200_ok_close = "HTTP/1.1 200\r\nConnection: close\r\n";
static std::string const s_content_len_name = "Content-Length: ";
static std::string const s_crlf = "\r\n";
static std::string const s_crlf2 = "\r\n\r\n";

int output_response(http_response_t *resp, int fd)
{
    conet::StringBuilder<4*1024> out;
    
    char buf[100];
    int len = 0;
    if (resp->http_code == 200) {
        if (resp->keepalive) {
            out.append(s_http_200_ok_keepalive);
        } else {
            out.append(s_http_200_ok_close);
        }
    } else {
        len = snprintf(buf, sizeof(buf), "HTTP/1.1 %d\r\n", resp->http_code);
        out.append(buf, len);
    }
    for (int i=0, n = (int)resp->headers.size(); i<n; ++i)
    {
        out.append(resp->headers[i]);
        out.append(s_crlf);
    }

    size_t blen =  resp->data_len;
    char const *data = resp->data; 
    if (data == NULL) {
        data = resp->body.data();
        blen = resp->body.size();
    }

    // Content-Length: %d\r\n
    out.append(s_content_len_name);
    out.append(blen);
    //len = snprintf(buf, sizeof(buf), "%ld", blen);
    //out.append(buf, len);
    out.append(s_crlf2);
    /*
    if (blen <= 4*1024) {
        out.append(data, blen);
        return send_data(fd, out.data(), out.size());
    } 

    // send header
    int ret = send_data(fd, out.data(), out.size());
    if (ret < 0) {
        return ret;
    }

    // send body
    return send_data(fd, data, blen);
*/

    struct iovec iov[2];
    iov[0].iov_base = (void *) out.data();
    iov[0].iov_len = out.size();
    iov[1].iov_base = (void *)data;
    iov[1].iov_len = blen;
    return writev(fd, iov, 2);
    
}


http_cmd_t * http_server_t::get_http_cmd(std::string const &name)
{
    AUTO_VAR(it, =, this->cmd_maps.find(name));
    if (it == this->cmd_maps.end())
    {
        if (this->default_cmd)
        {
            return this->default_cmd;
        }
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
        PLOG_ERROR("no found path cmd, ", (path));
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
        PLOG_ERROR("send response failed, ", (ret));
        return -1;
    }
    if (!http_server->enable_client_close  && (resp.keepalive == 0 || ctx.to_close))
    {
        return 1;
    }
    return 0;
}

int http_server_t::ws_main(conn_info_t *conn, http_request_t *req,
        tcp_server_t *server_base,
        http_server_t *http_server,
        char *buf,
        int len
        )
{

    if (this->enable_websocket == 0)
    {
        // server 不支持
        static char unsupport_txt[] =
            "HTTP/1.1 404\r\n"
            "Connection: close\r\n"
            "Contente-Length: 0\r\n"
            "\r\n";
        send(conn->fd, unsupport_txt, sizeof(unsupport_txt) - 1, 0);
        return 1;
    }

    websocket_conn_t *ws = new websocket_conn_t();
    ws->first_len = len;
    ws->first_buffer = buf;
    ws->conn_info = conn;
    ws->server = this;
    ws->http_first_req = req;
    ws->cb = this->ws_cb;
    ws->max_idle_time = this->websocket_max_idle_time;
    ws->server_masking_key = this->websocket_masking_key;
    ws->max_buf_len = this->websocket_max_packet_size;
    ws->buffer = new char[ws->max_buf_len];
    list_add(&ws->link_to, &this->conn_list);
    ws->do_handshake();
    list_del(&ws->link_to);
    delete ws;
    return 1;
}

int http_server_t::conn_proc(conn_info_t *conn)
{
    tcp_server_t *base_server = this->tcp_server;

    int fd  = conn->fd;

    int pmtu = IP_PMTUDISC_DO;
    setsockopt(fd, IPPROTO_IP, IP_MTU_DISCOVER, &pmtu, sizeof(pmtu));

    http_request_t req;

    int max_packet_size = base_server->conf.max_packet_size;
    int len = max_packet_size*2;

    char *buf = NULL;
    ssize_t nparsed = 0;
    ssize_t end =  0;
    int ret = 0;
    ssize_t recved = 0;
    int malloc_buff = 0;

    malloc_buff = 1;
    buf = (char *)aligned_alloc(64, len);

    char *recv_p = buf;
    int recv_len = 0;
    char *parse_p = buf;
    //int cnt = 0;

    while(ret == 0 && base_server->to_stop == 0) {
        if (!(recv_len > 0 && nparsed == 0)) {
            if (len-recv_len <=0) {
                PLOG_INFO("packet too long, [fd=",fd,"] [len=",len,"] [nparsed=",nparsed,"]");
                ret = -2;
                break;
            }

            recved = recv(fd, recv_p, len-recv_len, 0);
            if (recved == 0) {
                if (nparsed == 0) {
                    ret = 0;
                    break;
                }
                ret = -2;
                PLOG_INFO("recv failed [ret=",recved,"] [fd=",fd,"] [errno=",errno,"]");
                break;
            }
            if (recved < 0) {
                if (errno == EAGAIN || errno == EINTR) {
                    continue;
                }
                if (errno == ETIMEDOUT || errno == ECONNRESET) {
                    break;
                }
                PLOG_INFO("recv failed [ret=",recved,"] [fd=",fd,"] [errno=",errno,"] [errmsg=", strerror(errno), "]");
                ret = -2;
                break;
            }

            recv_p += recved;
            recv_len += recved; 
        }

        end = recv_p - parse_p;

        if (nparsed == 0) {
            //初始化 http req
            http_request_init(&req);
        }

        nparsed += http_request_parse(&req, parse_p, end, nparsed);
        ret = http_request_finish(&req);
        if (ret == 0) {
            continue;
        } else if (ret != 1) {
            ret = -1;
            break;
        }

        req.rest_data = parse_p + nparsed;
        req.rest_len = end - nparsed;

        if (req.websocket == 1) {
            ret = ws_main(conn, &req, base_server, this, buf, len);
            continue;
        }

        ret = http_server_main(conn, &req, base_server, this);
        if (ret != 0) {
            break;
        }

        parse_p += nparsed + req.content_length;
        if (recv_p - parse_p <= 0) {
            //  没有数据了, 重置
            parse_p = buf;
            recv_p = buf;
            recv_len = 0;
            nparsed = 0;
            continue;
        }

        if (parse_p - buf > max_packet_size) {
            //剩余数据的位置太后了, 先移动剩余数据
            recv_len = recv_p - parse_p;
            memmove(buf, parse_p, recv_len);
            parse_p = buf;
            recv_p = buf + recv_len;
        }
        nparsed = 0;
    } 

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
    /*
    if (this->cmd_maps.empty() ) {
        LOG(ERROR)<<"no cmd registried!";
        return -1;
    }
    */
    return this->tcp_server->start();
}

bool http_server_t::has_stoped()
{
    if (!tcp_server->has_stoped())
    {
        return false;
    }
    return true;
}

int http_server_t::do_stop()
{
    int ret = 0;
    if (tcp_server)
    {
        ret = tcp_server->stop();
    }
    return ret;
}

int http_server_t::registry_default_cmd(
        http_callback proc,
        void *arg,
        void (*free_arg)(void *arg)
        )
{
    http_cmd_t *item = new http_cmd_t;
    item->name = "";
    item->proc = proc;
    item->arg = arg;
    item->free_fn = free_arg;
    this->default_cmd = item;
    return 0;
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
    enable_websocket = 0;
    enable_client_close = 0;
    websocket_masking_key = 0x0;
    websocket_max_idle_time = 60; // 60 秒限制
    websocket_max_packet_size = 200*1024; // 默认 200K
    default_cmd = NULL;
    INIT_LIST_HEAD(&conn_list);
}

int http_server_t::set_ws_proc(websocket_cb_t const &cb)
{
    ws_cb = cb;
    return 0;
}

http_server_t::~http_server_t()
{
    for(__typeof__(cmd_maps.begin()) it=cmd_maps.begin(),
                iend = cmd_maps.end();
                it!=iend; ++it)
    {
        delete it->second;
    }

    if (this->default_cmd)
    {
        delete this->default_cmd;
        this->default_cmd = NULL;
    }
}

int http_server_t::set_ws_mask_key(uint32_t mask)
{
    websocket_masking_key = mask;
    return 0;
}

static ref_str_t g_ws_magic_num = ref_str("258EAFA5-E914-47DA-95CA-C5AB0DC85B11");

int websocket_conn_t::do_handshake()
{
    int ret = 0;
    this->fd = this->conn_info->fd;
    http_request_t &req = *this->http_first_req;

    SHA_CTX ctx;
    SHA1_Init(&ctx);
    SHA1_Update(&ctx, req.sec_websocket_key.data, req.sec_websocket_key.len);
    SHA1_Update(&ctx, g_ws_magic_num.data, g_ws_magic_num.len);
    char accept_key[SHA_DIGEST_LENGTH];
    SHA1_Final((unsigned char *)accept_key, &ctx);


    const int server_key_len = 28;
    char server_key_txt[30];
    conet::base64_encode(accept_key, SHA_DIGEST_LENGTH, server_key_txt);

    //send handshake
    static char handshake_rsp_1[] =
        "HTTP/1.1 101 Switching Protocols\r\n"
        "Upgrade: websocket\r\n"
        "Connection: Upgrade\r\n"
        "Sec-WebSocket-Accept: ";


    std::string rsp_txt(handshake_rsp_1, sizeof(handshake_rsp_1)-1);
    rsp_txt.append(server_key_txt, server_key_len);
    rsp_txt+="\r\n\r\n";
    ret = send(fd, rsp_txt.data(), rsp_txt.size(), 0);
    if (ret < (int) (rsp_txt.size()))
    {
        return -2;
    }

    // 握手成功
    //
    this->do_main();

    return 0;
}

int websocket_conn_t::do_main()
{
   //callback new
   int ret = 0;
   if (this->cb.do_new)
   {
       ret = this->cb.do_new(this->cb.arg, this);
       if (ret != 0)
       {
           PLOG_ERROR("websocket conn init failed,", (ret));
           // 不需要后续处理了
           return 0;
       }
   }

   this->fd = this->conn_info->fd;
   int &server_to_stop = this->server->to_stop;

   int read_stop = 0;
   int write_stop = 0;

   int32_t idle_cnt = 0;

   while(!server_to_stop)
   {
       if (to_close && this->send_queue.empty())
       {
           state = STOP;
           break;
       }
       struct pollfd pf = { 0 };
       pf.fd = fd;
       pf.events = (POLLERR|POLLHUP);
       pf.revents = 0;

       if (!read_stop && to_close == 0)
       {
           pf.events |= POLLIN;
       }
       else
       {
           if (this->ref_cnt <= 0)
           {
               break;
           }
       }

       if (!write_stop && !this->send_queue.empty())
       {
           pf.events |= POLLOUT;
       }

       int poll_interval = 10;
       ret = poll(&pf, 1, poll_interval);
       if (ret <0)
       {
           PLOG_ERROR("poll error, ", (ret));
           break;
       }
       else if (ret > 0)
       {
           if (pf.revents & POLLIN)
           {
               idle_cnt = 0;
               ret = do_read();
               if (ret <0)
               {
                   // socket 出错
                   PLOG_ERROR("read  data error, ", (ret));
                   break;
               }
               if (ret == 0)
               {
                   PLOG_INFO("close read by app client, ", (ret));
                   // 关闭了读
                   read_stop = 1;
                   decr_ref();
               }
           }
           if (pf.revents & POLLOUT)
           {
               idle_cnt = 0;
               ret = do_write();
               if (ret <0)
               {
                   // socket 出错了
                   PLOG_ERROR("write  data error, ", (ret));
                   break;
               }
           }
       }
       else
       {
           // 没有事件，空闲
           ++idle_cnt;
           if (idle_cnt * poll_interval >= this->max_idle_time * 1000)
           {
               PLOG_INFO("conn idle too much, would close, ",idle_cnt," seconds, [fd=",fd,"]");
               break;
           }
       }
   }

   if (this->cb.do_close)
   {
       this->cb.do_close(this->cb.arg, this);
   }

   PLOG_INFO("websocket conn exit,", (fd));
   return 0;
}

int websocket_conn_t::do_write()
{
    if (this->send_queue.empty())
    {
        return 0;
    }

    std::string * data = send_queue.front();
    int ret = 0;
    ret = send(fd, data->data()+ send_pos, data->size()- send_pos, MSG_DONTWAIT);
    if (ret <0)
    {
        if (errno == EINTR)
        {
            //中断打断, 下次继续
            return 0;
        }
        PLOG_ERROR("write data failed, ", (fd, ret, errno), "[errmsg=", strerror(errno),"]");
        return -1;
    }

    send_pos += ret;

    PLOG_INFO("send data [len=", send_pos, "]");
    if (send_pos >= (int32_t)data->size())
    {
        delete data;
        send_pos = 0;
        send_queue.pop_front();
    }
    return 0;
}

websocket_conn_t::websocket_conn_t()
{
    to_close = 0;
    INIT_LIST_HEAD(&link_to);
    state = READ_PAYLOAD_LEN;
    send_pos = 0;
    data_pos = 0;
    prev_data_pos = 0;
    max_buf_len = 200*1024;
    buffer = NULL;
    server_masking_key = 0;
    ref_cnt = 1;
    fd = -1;
    conn_info = NULL;
    http_first_req = NULL;
    server = NULL;
    first_buffer = NULL;
    first_len = 0;
    max_idle_time = -1;
    extend = NULL;
}

websocket_conn_t::~websocket_conn_t()
{
    delete[] buffer;
    while (!this->send_queue.empty())
    {
        delete this->send_queue.front();
        this->send_queue.pop_front();
    }
}

int websocket_conn_t::do_read()
{
    int ret = 0;
    int64_t rest_len = this->max_buf_len - this->data_pos;
    if (rest_len <= 0)
    {
        PLOG_ERROR("websocket packet too big, buff is full, [buff_size=", max_buf_len, "]");
        return -1;
    }
    ret = recv(fd, this->buffer+this->data_pos,
            this->max_buf_len - this->data_pos, MSG_DONTWAIT);
    if (ret <0)
    {
        if (errno == EINTR)
        {
            //中断打断, 下次继续
            return 1;
        }
        PLOG_ERROR("read data failed, ",(fd, ret, errno), "[errmsg=", strerror(errno),"]");
        return -1;
    }

    if (ret == 0)
    {
        PLOG_INFO("app has close conn,", (fd));
        return 0;
    }

    data_pos +=ret;

    while(1)
    {
        // 读取包的长度编码
        if (state == READ_PAYLOAD_LEN)
        {
            req_package.data = this->buffer + prev_data_pos;
            if (data_pos - prev_data_pos <2)
            {
                //LOG(INFO)<<"continue read packet len";
                //继续读包
                break;
            }

            memcpy(&req_package.head, this->buffer+prev_data_pos, 2);
            prev_data_pos += 2;
            state = PARSE_PAYLOAD_LEN;
        }

        if (state == PARSE_PAYLOAD_LEN)
        {  // 解析剩下的 payload_len
            if (req_package.head.payload_len == 126)
            {
                if ( data_pos - prev_data_pos >= 2)
                {
                    req_package.payload_len =  ntohs(*(uint16_t *)(this->buffer+prev_data_pos));
                    prev_data_pos += 2;
                    state = PARSE_MASKING;
                }
            }
            else if (req_package.head.payload_len == 127)
            {
                if ( data_pos - prev_data_pos >= 8)
                {
                    req_package.payload_len = be64toh(*(uint64_t *)(this->buffer+prev_data_pos));
                    prev_data_pos += 8;
                    state = PARSE_MASKING;
                }
            }
            else
            {
                req_package.payload_len = req_package.head.payload_len;
                state = PARSE_MASKING;
            }
        }

        if (state == PARSE_MASKING)
        { // 解析 masking_key
            if (req_package.head.MASK)
            {
                if (data_pos - prev_data_pos >= 4)
                {
                    req_package.masking_key = *(uint32_t *)(this->buffer+prev_data_pos);
                    prev_data_pos += 4;
                    state = CALC_REST_SIZE;
                }
                else
                {
                    break;
                }
            }
            else
            {
                state = CALC_REST_SIZE;
            }
        }

        if (state == CALC_REST_SIZE)
        {
            // 计算剩余空间
            if (max_buf_len - data_pos <  (int64_t) req_package.payload_len)
            {
                // 当前缓存保存不下了，
                //LOG(INFO)<<"move buffer, "
                //    "[fd="<<m_fd<<"]";
                memmove(buffer, buffer+prev_data_pos, data_pos - prev_data_pos);
                data_pos = data_pos - prev_data_pos;
                prev_data_pos = 0;
            }
            state = READ_DATA;
        }

        if (state == READ_DATA)
        {
            // 接收数据
            if (data_pos - prev_data_pos < (int64_t)req_package.payload_len)
            {
                // 包还没读完,
                break;
            }
            state = PROC_MSG;
        }

        if (state == PROC_MSG)
        {
            // 包已经接收完毕
            state = READ_PAYLOAD_LEN;
            req_package.payload_data = this->buffer + prev_data_pos;
            req_package.len = (this->buffer + prev_data_pos - req_package.data + req_package.payload_len);

            // 处理包
            ret = this->do_proc_pkg(&req_package);
            if (ret <0)
            {
                PLOG_ERROR("do proc frame failed!", (ret));
                return -1;
            }
            prev_data_pos+=req_package.payload_len;
        }
    }

    PLOG_INFO("leave read packet proc");

    return 1;
}

int websocket_conn_t::do_proc_pkg(ws_packet_t *pkg)
{

    int ret = 0;
    char * data = pkg->payload_data;
    uint64_t len = pkg->payload_len;
    uint8_t opcode = pkg->head.opcode;
    if (opcode == 0x9)
    {
        return do_pong(pkg);
    }

    if (opcode == 0x0 || pkg->head.FIN == 0)
    {
        PLOG_ERROR("package is divide, unsupport would closed!", (fd));
        this->to_close = 1;
        return 0;
    }

    // 解码 masking
    if (pkg->head.MASK)
    {
        uint64_t i = 0;
        char *masking_key = (char *) &pkg->masking_key;
        for (i=0; i<len; ++i)
        {
            data[i] ^= masking_key[i%4];
        }
    }

    ret = this->cb.do_msg(this->cb.arg, this, pkg);
    if (ret != 0)
    {
    }
    return 0;
}

int websocket_conn_t::do_pong(ws_packet_t *pkg)
{
    pkg->head.opcode = 0xA;
    return do_rsp(pkg);
}

int websocket_conn_t::do_rsp_close(char const *data, int32_t len)
{
    ws_packet_t rsp;
    rsp.head.opcode = 0x8;
    rsp.head.RSV = 0x0;
    rsp.head.FIN = 0x1;
    rsp.masking_key = 0;
    rsp.head.MASK = 0;

    if (data == NULL )
    {
        len = 0;
    }
    if (len == -1)
    {
        len = strlen(data);
    }

    uint64_t payload_len = len;
    if (payload_len <= 125)
    {
        rsp.head.payload_len = payload_len;
    }
    else if (payload_len <= 0xFFFF)
    {
        rsp.head.payload_len = 126;
    }
    else
    {
        rsp.head.payload_len = 127;
    }
    rsp.payload_len = len;
    rsp.payload_data = (char *)data;
    return do_rsp(&rsp);
}

int websocket_conn_t::do_rsp(char *data, uint64_t len, bool is_text)
{
    return do_rsp_with_mask(data, len, this->server_masking_key, is_text);
}

int websocket_conn_t::do_rsp_with_mask(char *data, uint64_t len,
            uint32_t mask, bool is_text)
{
    ws_packet_t rsp;
    if (is_text)
    {
        rsp.head.opcode = 0x1;
    }
    else
    {
        rsp.head.opcode = 0x2;
    }
    rsp.head.RSV = 0x0;
    rsp.head.FIN = 0x1;
    rsp.payload_len = len;
    rsp.payload_data = data;

    uint64_t payload_len = len;

    if (mask >0)
    { // 编码 masking
        uint64_t i = 0;
        char *masking_key = (char *)&mask;
        for (; i< len; ++i)
        {
           data[i] ^= masking_key[i%4];
        }

        rsp.head.MASK = 0x1;
        rsp.masking_key = mask;
    }
    else
    {
        rsp.head.MASK = 0x0;
        rsp.masking_key = 0;
    }

    if (payload_len <= 125)
    {
        rsp.head.payload_len = payload_len;
    }
    else if (payload_len <= 0xFFFF)
    {
        rsp.head.payload_len = 126;
    }
    else
    {
        rsp.head.payload_len = 127;
    }

    do_rsp(&rsp);

    return 0;
}

int websocket_conn_t::do_rsp(ws_packet_t * pkg)
{
    uint64_t payload_len = pkg->payload_len;
    uint64_t total_len = 2 + payload_len;
    uint32_t mask = pkg->masking_key;
    if (mask > 0)
    {
        total_len += 4;
    }

    if (payload_len > 0xFFFF)
    {
        total_len += 8;
    }
    else if (payload_len > 125)
    {
        total_len += 2;
    }

    std::string *out = new std::string();
    out->resize(total_len);

    char * p = (char *)out->data();
    memcpy(p, &pkg->head, 2);
    p+=2;

    if (125 < payload_len  && payload_len <= 0xFFFF)
    {
        uint16_t o_len = htons((uint16_t)(payload_len));
        memcpy(p, &o_len, sizeof(o_len));
        p+=2;
    }
    else if (payload_len > 0xFFFF)
    {
        uint64_t o_len = htonl(payload_len);
        memcpy(p, &o_len, sizeof(o_len));
        p+=8;
    }

    if ( mask > 0)
    {
        *(uint32_t *)(p) = mask;
        p+=sizeof(mask);
    }

    memcpy(p, pkg->payload_data, pkg->payload_len);

    this->send_queue.push_back(out);
    return 0;
}

}
