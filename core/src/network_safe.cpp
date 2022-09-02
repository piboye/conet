#include <sys/epoll.h>
#include <poll.h>
#include <vector>
#include <algorithm>
#include <assert.h>
#include <sys/resource.h>

#include "coroutine.h"
#include "coroutine_impl.h"
#include "timewheel.h"
#include "log.h"
#include "hook_helper.h"
#include "timewheel.h"
#include "network_hook.h"
#include "dispatch.h"
#include "gflags/gflags.h"

#include "../../base/list.h"
#include "../../base/tls.h"

DEFINE_int32(epoll_size, 10000, "epoll event size ");


HOOK_DECLARE(
    int, epoll_wait,(int epfd, struct epoll_event *events,
                     int maxevents, int timeout)
);


namespace conet
{

struct poll_ctx_t;

struct poll_wait_item_t
{
    poll_ctx_t * poll_ctx;
    int  pos;
    int fd;
    uint32_t wait_events;
    poll_wait_item_t()
    {
        poll_ctx = NULL;
        pos = 0;
        wait_events = 0;
    }
};

struct poll_wait_item_mgr_t
{
    poll_wait_item_t ** wait_items;
    int size;

    static int default_size;
    static 
    int get_default_size()
    {
        if (default_size == 0) {
            struct rlimit rl;
            int ret = 0;
            ret = getrlimit( RLIMIT_NOFILE, &rl);
            if (ret) {
                default_size = 100000;
            } else {
                default_size = rl.rlim_max;
            }
        }
        return default_size;
    }

    poll_wait_item_mgr_t() 
    {
        this->size = get_default_size(); 
        wait_items = (poll_wait_item_t **) malloc(sizeof(poll_wait_item_t *)* (size+1));
        memset(wait_items, 0, (size+1)*sizeof(void *));
    }

    int expand(int need_size)
    {
        int new_size = size;
        while (new_size <= need_size) {
            new_size +=10000;
        }
        poll_wait_item_t **ws = (poll_wait_item_t **) malloc(sizeof(poll_wait_item_t *)* (new_size+1));
        memset(ws, 0, (new_size+1)*sizeof(void *));
        memcpy(ws, this->wait_items, (this->size+1) * sizeof(void *) ); 
        this->size = new_size;
        free(this->wait_items);
        this->wait_items = ws;
        return new_size;
    }

    ~poll_wait_item_mgr_t()
    {
        for(int i=0;i<= this->size; ++i) 
        {
            if (this->wait_items[i]) {
                delete this->wait_items[i];
            }
        }
        free(this->wait_items);
    }
};

int poll_wait_item_mgr_t::default_size = 0;

static __thread  poll_wait_item_mgr_t * g_wait_item_mgr = NULL;
CONET_DEF_TLS_VAR_HELP_DEF(g_wait_item_mgr);

poll_wait_item_t * get_wait_item(int fd) 
{
    poll_wait_item_mgr_t *mgr = TLS_GET(g_wait_item_mgr);
    if (fd < 0) {
        return NULL;
    }
    if ( fd >= mgr->size)
    {
        mgr->expand(fd);
    }
    poll_wait_item_t *wait_item = mgr->wait_items[fd];
    if (NULL == wait_item )
    {
        wait_item = new poll_wait_item_t();
        mgr->wait_items[fd] = wait_item;
        wait_item->fd = fd;
        wait_item->wait_events = 0;
        wait_item->poll_ctx = NULL;
        wait_item->pos = 0;
    }
    return wait_item;
}

struct poll_ctx_t
{
    struct pollfd *fds;
    nfds_t nfds;

    int all_event_detach;

    int num_raise;

    int retcode;

    coroutine_t * coroutine;

    timeout_handle_t timeout_ctl;

    list_head to_dispatch; // fd 有事件的时候， 把这个poll加入到 分发中
};


epoll_ctx_t * get_epoll_ctx();

void init_poll_wait_item(poll_wait_item_t *self, poll_ctx_t *ctx, int pos)
{
    if (self->poll_ctx != NULL)
    {
        LOG_FATAL("this fd, has been polled by other");
        exit(1);
    }
    self->poll_ctx = ctx;
    self->pos = pos;
}



void poll_ctx_timeout_proc(void *arg)
{
    poll_ctx_t *self = (poll_ctx_t *)(arg);
    self->retcode = 1;
    if (self->coroutine) {
        resume(self->coroutine);
    }
    return ;
}

void close_fd_notify_poll(int fd)
{
}

inline 
void fd_notify_events_to_poll(poll_wait_item_t *wait_item, uint32_t events, list_head *dispatch, int epoll_fd)
{
    int pos = wait_item-> pos;

    int fd = wait_item->fd;

    int ret =0;
    poll_ctx_t *poll_ctx = wait_item->poll_ctx;
    if (NULL == poll_ctx)
    {
        epoll_event ev;
        ev.events = 0;
        ev.data.ptr = wait_item;
        ret = epoll_ctl(epoll_fd, EPOLL_CTL_DEL, wait_item->fd,  &ev);
        if (ret) {
            PLOG_ERROR(" epoll_ctl_del failed, ", (fd, ret, errno), "[errmsg=", strerror(errno),"]");
        }
        return;
    }

    int nfds = (int) poll_ctx->nfds;
    if ( (pos < 0) || ( nfds <= pos) ) {
        PLOG_ERROR("error fd ctx [pos:", pos, "]");
        return;
    }

    struct pollfd * fds = poll_ctx->fds;
    uint32_t mask = fds[pos].events;
    uint32_t revents = (mask & events);
    fds[pos].revents |= revents;
    cancel_timeout(&poll_ctx->timeout_ctl);
    // add to dispatch
    list_move_tail(&poll_ctx->to_dispatch, dispatch);
    if (revents) ++poll_ctx->num_raise;
}

void  init_poll_ctx(poll_ctx_t *self, pollfd *fds, nfds_t nfds, epoll_ctx_t *epoll_ctx);

epoll_ctx_t *create_epoll(int event_size);


static uint32_t poll_event2epoll( short events )
{
    uint32_t e = 0;
    if( events & POLLIN ) 	e |= EPOLLIN;
    if( events & POLLOUT )  e |= EPOLLOUT;
    if( events & POLLHUP ) 	e |= EPOLLHUP;
    if( events & POLLERR )	e |= EPOLLERR;
    return e;
}

static short epoll_event2poll( uint32_t events )
{
    short e = 0; 
    if( events & EPOLLIN ) 	e |= POLLIN;
    if( events & EPOLLOUT ) e |= POLLOUT;
    if( events & EPOLLHUP ) e |= POLLHUP;
    if( events & EPOLLERR ) e |= POLLERR;
    return e;
}



void init_epoll_ctx(epoll_ctx_t *self, int size)
{
    self->m_epoll_size = size;
    self->m_epoll_events = new epoll_event[size];
    self->m_epoll_fd = epoll_create(size);
    self->wait_num = 0;
    return;
}

void  init_poll_ctx(poll_ctx_t *self,
                    pollfd *fds, nfds_t nfds, epoll_ctx_t *epoll_ctx)
{
    self->fds = fds;
    self->nfds = nfds;
    self->num_raise = 0;
    self->all_event_detach = 0;
    self->coroutine = NULL;
    self->retcode = 0;

    INIT_LIST_HEAD(&self->to_dispatch);

    int ret  = 0;
    int epoll_fd = epoll_ctx->m_epoll_fd;
    for(int i=0; i< (int)nfds; ++i) 
    {
        int fd= fds[i].fd;
        fds[i].revents = 0;
        if( fd > -1 ) 
        {
            poll_wait_item_t *wait_item = get_wait_item(fd);
            if (wait_item) 
            {
                init_poll_wait_item(wait_item,  self, i);
                epoll_event ev;
                ev.events = poll_event2epoll( fds[i].events);
                ev.data.ptr = wait_item;
                ret = epoll_ctl(epoll_fd, EPOLL_CTL_ADD, fd,  &ev);
                if (ret) {
                    PLOG_ERROR("epoll_ctl_add failed, ", (fd, ret, errno), "[errmsg=", strerror(errno),"]");
                }
            } else {
                PLOG_ERROR("get wait item failed, [fd=", fd, "]");
            }
        } else {
            PLOG_ERROR("error fd, [fd=", fd, "]");
        }
    }

}

void destruct_poll_ctx(poll_ctx_t *self, epoll_ctx_t * epoll_ctx)
{
    for(int i=0; i< (int)self->nfds; ++i)
    {
        poll_wait_item_t *wait_item = get_wait_item(self->fds[i].fd);
        wait_item->poll_ctx = NULL;
        epoll_ctl(epoll_ctx->m_epoll_fd, EPOLL_CTL_DEL, wait_item->fd, NULL);
    }
}

int proc_netevent(epoll_ctx_t * epoll_ctx, int timeout)
{
    if (!epoll_ctx) return -1;

    int ret = 0;
    int cnt = 0;
    ret = _(epoll_wait)(epoll_ctx->m_epoll_fd, &epoll_ctx->m_epoll_events[0],
                     epoll_ctx->m_epoll_size, timeout);
    if (ret <0 ) {
        // epoll_wait failed;
        if (errno != 4) {
            LOG_SYS_CALL(epoll_wait, ret);
        }
        return 0;
    }
    if (ret ==0 ) {
        return ret;
    }

    if (ret > epoll_ctx->m_epoll_size ) ret = epoll_ctx->m_epoll_size;

    list_head dispatch;
    INIT_LIST_HEAD(&dispatch);
    for (int i=0; i<ret; ++i) {
        poll_wait_item_t * wait_item = (poll_wait_item_t *)(epoll_ctx->m_epoll_events[i].data.ptr);
        int events = epoll_event2poll(epoll_ctx->m_epoll_events[i].events);
        fd_notify_events_to_poll(wait_item, events, &dispatch, epoll_ctx->m_epoll_fd);
    }

    list_head *it=NULL, *next=NULL;
    list_for_each_safe(it, next, &dispatch)
    {
        list_del_init(it);
        poll_ctx_t * ctx = container_of(it, poll_ctx_t, to_dispatch);
        if (ctx->coroutine) {
            cnt++;
            resume(ctx->coroutine);
        }
    }
    return cnt;
}

int proc_netevent(int timeout)
{
    epoll_ctx_t * epoll_ctx =  get_epoll_ctx();
    return proc_netevent(epoll_ctx, timeout);
}

int task_proc(void *arg)
{
    return proc_netevent((epoll_ctx_t *) arg, -1);
}

epoll_ctx_t *create_epoll(int event_size)
{
    epoll_ctx_t * p = new epoll_ctx_t;
    init_epoll_ctx(p, event_size);
    registry_task(task_proc, p);
    return p;
}

int co_poll(struct pollfd fds[], nfds_t nfds, int timeout)
{
    epoll_ctx_t *epoll_ctx  = get_epoll_ctx();
    {
        poll_ctx_t poll_ctx;
        init_poll_ctx(&poll_ctx, fds, nfds, epoll_ctx);

        init_timeout_handle(&poll_ctx.timeout_ctl, poll_ctx_timeout_proc, &poll_ctx);
        if (timeout >= 0) {
            set_timeout(&poll_ctx.timeout_ctl, timeout);
        }

        poll_ctx.coroutine = current_coroutine();

        ++ epoll_ctx->wait_num;
        yield();
        -- epoll_ctx->wait_num;

        destruct_poll_ctx(&poll_ctx, epoll_ctx);

        switch (poll_ctx.retcode)
        {
            case 0: // 有事件
                return poll_ctx.num_raise;
            case 1: // 超时
                return 0;
            case 2: // 出错了
                return -1;
            default:
                return -1;
        }
    }
}


void free_epoll(epoll_ctx_t *ep)
{
    delete [] ep->m_epoll_events;
    delete ep;
}

int get_epoll_pend_task_num() 
{
    return get_epoll_ctx()->wait_num;
}

__thread epoll_ctx_t * g_epoll_ctx = NULL;

CONET_DEF_TLS_GET(g_epoll_ctx, create_epoll(FLAGS_epoll_size), free_epoll);

epoll_ctx_t * get_epoll_ctx()
{
    return TLS_GET(g_epoll_ctx);
}


}

