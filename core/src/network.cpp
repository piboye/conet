#include <sys/epoll.h>
#include <poll.h>
#include <vector>
#include <algorithm>
#include <assert.h>

#include "coroutine.h"
#include "coroutine_impl.h"
#include "timewheel.h"
#include "log.h"
#include "hook_helper.h"
#include "timewheel.h"
#include "network_hook.h"
#include "dispatch.h"
#include "thirdparty/gflags/gflags.h"

#include "base/incl/list.h"
#include "base/incl/tls.h"
#include <sys/resource.h>

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
    list_head link_to;
    int  pos;
    uint32_t wait_events;
    poll_wait_item_t()
    {
        INIT_LIST_HEAD(&link_to);
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
        return new_size;
    }

    ~poll_wait_item_mgr_t()
    {
        for(int i=0;i<= this->size; ++i) 
        {
            if (this->wait_items[i]) {
                delete wait_items[i];
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
        return NULL;
    }
    poll_wait_item_t *wait_item = mgr->wait_items[fd];
    if (NULL == wait_item )
    {
        wait_item = new poll_wait_item_t();
        mgr->wait_items[fd] = wait_item;
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

    list_head wait_queue;
    list_head to_dispatch; // fd 有事件的时候， 把这个poll加入到 分发中
};


epoll_ctx_t * get_epoll_ctx();

void init_poll_wait_item(poll_wait_item_t *self, poll_ctx_t *ctx, int pos)
{
    if (self->poll_ctx != NULL)
    {
        LOG(FATAL)<<"this fd, has been polled by other";
        exit(1);
    }
    list_add_tail(&self->link_to, &ctx->wait_queue);
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
    poll_wait_item_t *wait_item = get_wait_item(fd);
    if (wait_item && wait_item->poll_ctx)  
    {
        wait_item->poll_ctx->retcode = 2;
        epoll_ctx_t *ep_ctx = get_epoll_ctx();
        epoll_ctl(ep_ctx->m_epoll_fd, fd, EPOLL_CTL_DEL, NULL);
        conet::resume(wait_item->poll_ctx->coroutine);
    }
}
void fd_notify_events_to_poll(poll_wait_item_t *wait_item, uint32_t events, list_head *dispatch, int epoll_fd)
{
    uint32_t rest_events  = 0;
    int pos = wait_item-> pos;
    poll_ctx_t *poll_ctx = wait_item->poll_ctx;
    int nfds = (int) poll_ctx->nfds;
    if ( (pos < 0) || ( nfds <= pos) ) {
        LOG(ERROR)<<"error fd ctx pos";
        return;
    }

    struct pollfd * fds = poll_ctx->fds;
    // set reachable events
    uint32_t mask = fds[pos].events;
    uint32_t revents = (mask & events);
    if (revents && dispatch) {
        fds[pos].revents |= revents;
        // add to dispatch
        list_move_tail(&poll_ctx->to_dispatch, dispatch);
        ++poll_ctx->num_raise;
        cancel_timeout(&poll_ctx->timeout_ctl);
    }  else {
        // rest events in here
        rest_events |= mask;
    }
    // increase fd num

    int fd = fds[pos].fd;
    uint32_t wait_events = wait_item->wait_events;
    if ((revents != events) ||  //有网络事件， 但是没有侦听者， 需要去除，不然会cpu 100%
            (((wait_events & EPOLLOUT) > 0) && ((rest_events & EPOLLOUT)== 0 )) // EPOLLOUT 如果没有人等待， 就删除
       )
    {
        epoll_event ev;
        uint64_t diff_events = events & (~revents);
        rest_events &= (~diff_events);
        if (rest_events & EPOLLOUT) 
        {
            rest_events &= ~EPOLLOUT;
        }
        if (rest_events) {
            wait_item->wait_events = rest_events;
            ev.events = rest_events;
            ev.data.ptr = wait_item;  
            epoll_ctl(epoll_fd, EPOLL_CTL_MOD, fd,  &ev);
        } else {
            ev.events = 0;
            wait_item->wait_events= 0;
            epoll_ctl(epoll_fd, EPOLL_CTL_DEL, fd,  &ev);
        }
    }
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
    INIT_LIST_HEAD(&self->wait_queue);

    for(int i=0; i< (int)nfds; ++i) 
    {
        fds[i].revents = 0;
        if( fds[i].fd > -1 ) 
        {
            poll_wait_item_t *wait_item = get_wait_item(fds[i].fd);
            if (wait_item) 
            {
                init_poll_wait_item(wait_item,  self, i);
                uint32_t wait_events = wait_item->wait_events;
                epoll_event ev;
                ev.events = poll_event2epoll( fds[i].events);
                ev.data.ptr = wait_item;
                if (wait_events) { // 已经设置过EPOLL事件
                    uint32_t events = ev.events;
                    events |= wait_events;
                    if (events != ev.events) { // 有变化， 修改事件
                        ev.events = events;
                        epoll_ctl(epoll_ctx->m_epoll_fd, EPOLL_CTL_MOD, fds[i].fd,  &ev);
                        wait_item->wait_events = ev.events;
                    } 
                } else {
                    // 新句柄
                    epoll_ctl(epoll_ctx->m_epoll_fd, EPOLL_CTL_ADD, fds[i].fd,  &ev);
                    wait_item->wait_events = ev.events;
                }
            }
        }
    }

    init_timeout_handle(&self->timeout_ctl, poll_ctx_timeout_proc, self);
}

void destruct_poll_ctx(poll_ctx_t *self, epoll_ctx_t * epoll_ctx)
{
    poll_wait_item_t *item=NULL, *next= NULL;
    list_for_each_entry_safe(item, next, &self->wait_queue, link_to)
    {
        list_del_init(&item->link_to);
        item->poll_ctx = NULL;
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

int remove_epoll_ctl(int fd)
{
    epoll_ctx_t *ctx = get_epoll_ctx(); 
    return epoll_ctl(ctx->m_epoll_fd, fd, EPOLL_CTL_DEL, NULL);
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

