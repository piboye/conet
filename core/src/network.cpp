#include <sys/epoll.h>
#include <poll.h>
#include <algorithm>
#include "coroutine.h"
#include "coroutine_impl.h"
#include <vector>
#include "timewheel.h"
#include <assert.h>
#include "list.h"
#include "fd_ctx.h"
#include "log.h"
#include "hook_helper.h"
#include "timewheel.h"
#include "tls.h"
#include "network_hook.h"
#include "dispatch.h"

HOOK_DECLARE(
int, epoll_wait,(int epfd, struct epoll_event *events,
                  int maxevents, int timeout)
);


namespace conet 
{

struct poll_ctx_t;

struct poll_wait_item_t
{
    list_head to_fd;
    poll_ctx_t * poll_ctx;
    int  pos;
    fd_ctx_t *fd_ctx;
};

struct poll_ctx_t
{
	struct pollfd *fds;
	nfds_t nfds; 

    poll_wait_item_t wait_items_cache[2];
    poll_wait_item_t *wait_items;

	int all_event_detach;

	int num_raise;

	int is_timeout;

	coroutine_t * coroutine;

	timeout_handle_t timeout_ctl;
    
	list_head to_dispatch; // fd 有事件的时候， 把这个poll加入到 分发中
};

struct epoll_ctx_t
{
	int m_epoll_fd;
	int m_epoll_size;

	epoll_event *m_epoll_events;

    int wait_num;
};

epoll_ctx_t * get_epoll_ctx();

void init_poll_wait_item(poll_wait_item_t *self, poll_ctx_t *ctx, int pos, fd_ctx_t * fd_ctx)
{
    INIT_LIST_HEAD(&self->to_fd);
    self->poll_ctx = ctx;
    self->pos = pos;
    self->fd_ctx = fd_ctx;
    list_add_tail(&self->to_fd, &fd_ctx->poll_wait_queue);
}



void poll_ctx_timeout_proc(void *arg)
{
     poll_ctx_t *self = (poll_ctx_t *)(arg);
     int nfds = (int) self->nfds;
     for(int i=0; i< (int)nfds; ++i) {
         list_del_init(&self->wait_items[i].to_fd);
     }
     self->is_timeout = 1;
     if (self->coroutine) {
         resume(self->coroutine);
     }
     return ;
}

void fd_notify_events_to_poll(fd_ctx_t *fd_ctx, uint32_t events, list_head *dispatch, int epoll_fd)
{
    list_head *it=NULL, *next=NULL;

    uint32_t rest_events  = 0;

    list_for_each_safe(it, next, &fd_ctx->poll_wait_queue)
    {
        assert(it);
        poll_wait_item_t * item = container_of(it, poll_wait_item_t, to_fd);
        int pos = item-> pos;
        poll_ctx_t *poll_ctx = item->poll_ctx;
        int nfds = (int) poll_ctx->nfds;
        if ( (pos < 0) || ( nfds <= pos) ) {
            assert(!"error fd ctx pos");
            continue;
        }
        
        struct pollfd * fds = poll_ctx->fds;
        // set reachable events
        uint32_t mask = fds[pos].events;
        uint32_t revents = (mask & events); 
        if (revents) {
            /*
            CONET_LOG(DEBUG, "fd:%d, need evets:%d, events:%d, mask:%u, revents:%u", \
                fd_ctx->fd, 
                fds[pos].events,
                events, mask, revents);
            */
            fds[pos].revents |= revents;
            // add to dispatch
            list_del_init(&poll_ctx->to_dispatch);
            list_add_tail(&poll_ctx->to_dispatch, dispatch);
	        ++poll_ctx->num_raise;
            cancel_timeout(&poll_ctx->timeout_ctl); 
            list_del_init(it);
        }  else {
            // rest events in here
            rest_events |= mask;
        }
        // increase fd num
	}
    if (rest_events) {
        epoll_event ev;
        ev.events = rest_events;
        ev.data.ptr = fd_ctx;
        fd_ctx->wait_events = rest_events;
        epoll_ctl(epoll_fd, EPOLL_CTL_MOD, fd_ctx->fd,  &ev);
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
    self->m_epoll_events = (epoll_event *)malloc(sizeof(epoll_event)* size);
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
	self->is_timeout = 0;
    
    if (nfds > 2) {
        self->wait_items = (poll_wait_item_t *) malloc(sizeof (poll_wait_item_t) * nfds);
    } else {
        self->wait_items =  self->wait_items_cache;
    }

	INIT_LIST_HEAD(&self->to_dispatch);

	for(int i=0; i< (int)nfds; ++i) {
        if( fds[i].fd > -1 ) {
            fds[i].revents = 0;
            CONET_LOG(DEBUG, "poll wait fd:%d, events:%u", fds[i].fd, fds[i].events);
            fd_ctx_t *item = get_fd_ctx(fds[i].fd);
            if (item) {
                incr_ref_fd_ctx(item);
                init_poll_wait_item(self->wait_items+i,  self, i,  item);

                epoll_event ev;
                ev.events = poll_event2epoll( fds[i].events);
                ev.data.ptr = item;
                if (item->wait_events == 0)  {
                    item->wait_events = ev.events;
                    epoll_ctl(epoll_ctx->m_epoll_fd, EPOLL_CTL_ADD, fds[i].fd,  &ev);
                } else {
                    // change events;
                    ev.events |= item->wait_events;
                    item->wait_events = ev.events;
                    epoll_ctl(epoll_ctx->m_epoll_fd, EPOLL_CTL_MOD, fds[i].fd,  &ev);
                }
            }	
        }
	}

	init_timeout_handle(&self->timeout_ctl, poll_ctx_timeout_proc, self);
}

void destruct_poll_ctx(poll_ctx_t *self, epoll_ctx_t * epoll_ctx)
{
	for(int i=0; i< (int)self->nfds; ++i) {

        if(self->fds[i].fd > -1 )
        {
            list_del_init(&self->wait_items[i].to_fd);

            fd_ctx_t *fd_ctx = self->wait_items[i].fd_ctx;
            if (list_empty_careful(&fd_ctx->poll_wait_queue)) {
                epoll_event ev;
                ev.events = fd_ctx->wait_events;
                ev.data.ptr = fd_ctx;
                fd_ctx->wait_events = 0;
                epoll_ctl(epoll_ctx->m_epoll_fd, EPOLL_CTL_DEL, self->fds[i].fd,  &ev);
            }

            decr_ref_fd_ctx(fd_ctx);
        }	
	}

    if (self->nfds > 2)
    {
        free(self->wait_items);
        self->wait_items = NULL;
    }

}





int proc_netevent(epoll_ctx_t * epoll_ctx, int timeout)
{
    if (!epoll_ctx) return -1;

    int ret = 0;
    int cnt = 0;
    ret = epoll_wait(epoll_ctx->m_epoll_fd, &epoll_ctx->m_epoll_events[0],
            epoll_ctx->m_epoll_size, timeout);
    if (ret <0 ) return ret;
    if (ret ==0 ) {
        return ret;
    }

    if (ret > epoll_ctx->m_epoll_size ) ret = epoll_ctx->m_epoll_size;

    list_head dispatch;
    INIT_LIST_HEAD(&dispatch);
    for (int i=0; i<ret; ++i) {
        fd_ctx_t * fd_ctx = (fd_ctx_t *)epoll_ctx->m_epoll_events[i].data.ptr;
        int events = epoll_event2poll(epoll_ctx->m_epoll_events[i].events);
        if (fd_ctx) {
            fd_notify_events_to_poll(fd_ctx, events, &dispatch, epoll_ctx->m_epoll_fd);
        }
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
    return proc_netevent((epoll_ctx_t *) arg, 1);
}

epoll_ctx_t *create_epoll(int event_size)
{
	epoll_ctx_t * p = (epoll_ctx_t *) malloc(sizeof(epoll_ctx_t));
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

		if (poll_ctx.is_timeout) {
			return 0;
		}
		return poll_ctx.num_raise;
	}
}



void free_epoll(epoll_ctx_t *ep)
{
    delete ep;
}

int get_epoll_pend_task_num() {
    return get_epoll_ctx()->wait_num;
}

__thread epoll_ctx_t * g_epoll_ctx = NULL;

DEF_TLS_GET(g_epoll_ctx, create_epoll(1000), free_epoll)

epoll_ctx_t * get_epoll_ctx()
{
    return tls_get(g_epoll_ctx); 
}


}

