#include <sys/eventfd.h>
#include <poll.h>
#include <stdint.h>
#include <stdio.h>
#include <unistd.h>
#include <fcntl.h>
#include <sys/types.h>
#include <sys/stat.h>
#include "../log_format.h"
#include "../plog.h"
#include "../time_helper.h"
#include "thirdparty/gflags/gflags.h"
#include "time_helper.h"
#include <sys/syscall.h>

#define OUT_OF_INDEXES          0xfffffff

DEFINE_int32(log_type, 1, "0 stderr, 1 rotate log");
DEFINE_string(log_file, "my.log", "base log filename");
DEFINE_int32(log_max_file_num, 10, "max file num");
DEFINE_int32(log_max_file_size, 10, "max file size default 10 MB");
DEFINE_string(log_level, "INFO", "log level: DEBUG, INFO, WRAN, ERROR");
using namespace conet;

namespace conet
{
static int log_level_cast(std::string  const & a_str)
{
    char const * str  = a_str.c_str();
    if (NULL ==str) str = "";
#define LOG_LEVEL_STR_CAST(level) if (strcasecmp(str, #level) == 0) return PLog::level
    LOG_LEVEL_STR_CAST(DEBUG);
    LOG_LEVEL_STR_CAST(INFO);
    LOG_LEVEL_STR_CAST(WARN);
    LOG_LEVEL_STR_CAST(ERROR);
    LOG_LEVEL_STR_CAST(ALERT);
#undef LOG_LEVEL_STR_CAST
    return PLog::DEBUG;
}

static char const * get_level_str(int level)
{
    switch(level)
    {
        case PLog::DEBUG :
            return "DEBUG";
        case PLog::INFO :
            return "INFO";
        case PLog::WARN :
            return "WARN";
        case PLog::ERROR :
            return "ERROR";
        case PLog::ALERT :
            return "ALERT";
        default:
            return "DEBUG";
    }
}


PLog::PLog()
{
    init_llist_head(&m_request_queue);
    m_queue_len = 0;
    m_initialized = 0;
    m_stop_flags = 0;
    m_work_notify = -1;
    m_main_thread = 0;
    m_log_type = STDERR_LOG;
    m_log_level = INFO;
    m_fd = 2;
    m_color_key = OUT_OF_INDEXES;

    init_llist_head(&m_color_all);
    pthread_key_create(&this->m_color_key, NULL);
    if (this->m_color_key == 0) {
        printf("create plog color key is zero!\n");
        pthread_key_create(&this->m_color_key, NULL);
    }

    m_work_notify = eventfd(0, EFD_CLOEXEC|EFD_NONBLOCK);
    if (m_work_notify<0)
    {
        abort();
        return;
    }

    m_max_file_num = 10;
    m_max_file_size = 100*1024*1024;
    m_prev_check_timestamp = 0;
}

PLog::~PLog()
{
    m_stop_flags = 1;
    if (m_main_thread)
    {
        pthread_join(m_main_thread, NULL);
    }
    CleanUp();
    if (m_fd > 2)
    {
        close(m_fd);
        m_fd = -1;
    }

    if (m_work_notify >=0)
    {
        close(m_work_notify);
        m_work_notify = -1;
    }

    {
        // 删除所有的 color
        color_cb_t *it=NULL, *next=NULL;
        llist_for_each_entry_safe(it, next, m_color_all.first, link_to)
        {
            delete it;
        }
        m_color_all.first = NULL;
        if (m_color_key) pthread_key_delete(m_color_key);
    }
}

static PLog g_plog;

PLog & PLog::Instance()
{
    return g_plog;
}

int PLog::Add(ReqItem *item)
{
    if (m_stop_flags)
    {
        return -1;
    }

    gettimeofday(&item->timestamp, NULL);
    item->thread_id = syscall(__NR_gettid);

    item->link_to.next = NULL;
    llist_add(&item->link_to, &m_request_queue);

    int64_t num = __sync_fetch_and_add(&m_queue_len, 1);
    if (num <= 0)
    {
        uint64_t wv = 1;
        write(m_work_notify, &wv, sizeof(wv));
    }
    return 0;
}

int PLog::AddFast(ReqItem *item)
{
    gettimeofday(&item->timestamp, NULL);
    item->thread_id = syscall(__NR_gettid);
    item->link_to.next = NULL;

    struct tm *tl = localtime(&item->timestamp.tv_sec);

    char tm_txt[100]={0};
    char tm_rest[20]={0};
    strftime(tm_txt, 100, "[%Y%m%d %H:%M:%S ", tl);
    snprintf(tm_rest, 20, "%06d", (int)item->timestamp.tv_usec);
    std::string out_txt;
    LOG_FORMAT(out_txt, tm_txt, tm_rest, "]"
            "(", item->thread_id, ")"
            "[", item->file_name, ":", item->line, "]"
            "[", item->func, "]"
            "[", get_level_str(item->level), "]: ",
            item->text);
    if (m_fd != 2)
    {
        write(2, out_txt.c_str(), out_txt.size());
    }
    if (m_fd >= 0)
    {
        write(m_fd, out_txt.c_str(), out_txt.size());
    }
    delete item;
    return 0;
}

int PLog::ProcLog(llist_node *queue)
{
    queue = llist_reverse_order(queue);
    llist_node * it = NULL, *next = NULL;
    char tm_txt[100]={0};
    char tm_rest[20]={0};
    for(it = queue; it; it = next)
    {
        next = it->next;
        it->next = NULL;
        ReqItem *item = container_of(it, ReqItem, link_to);
        struct tm *tl = localtime(&item->timestamp.tv_sec);
        strftime(tm_txt, 100, "[%Y%m%d %H:%M:%S ", tl);
        snprintf(tm_rest, 20, "%06d", (int)item->timestamp.tv_usec);
        std::string out_txt;
        LOG_FORMAT(out_txt, tm_txt, tm_rest, "]"
                "(", item->thread_id, ")"
                "[", item->file_name, ":", item->line, "]"
                "[", item->func, "]"
                "[", get_level_str(item->level), "]: ",
                item->text);
        write(m_fd, out_txt.c_str(), out_txt.size());
        __sync_fetch_and_add(&m_queue_len, -1);
        delete item;
    }
    return 0;
}

int PLog::Start()
{
    int ret = 0;
    ret = pthread_create(&m_main_thread, NULL, &MainProcHelp, this);
    return ret;
}


int PLog::Stop()
{
    m_stop_flags = 1;
    return 0;
}

int PLog::MainProc()
{
    int ret = 0;
    uint64_t cnt = 0;
    while (!m_stop_flags)
    {
        if (!llist_empty(&m_request_queue))
        {
            ++cnt;
            llist_node *queue = llist_del_all(&m_request_queue);
            ProcLog(queue);
        }
        else
        {
            // 休眠 1 ms
            struct pollfd pf = {0};
            pf.fd = m_work_notify;
            pf.events = (POLLIN|POLLERR|POLLHUP);
            ret = poll(&pf, 1, 1);
            uint64_t num = 0;
            if (ret > 0)
            {
                ret = read(m_work_notify, &num, sizeof(num));
                if (ret < 0)
                {
                    continue;
                }
            }
        }
        if (cnt % 10 == 0)
        {
            uint64_t now = get_sys_ms();
            if (now - m_prev_check_timestamp > 1000)
            {
                check_log();
            }
        }
    }

    usleep(1000*10); // 休眠10ms
    CleanUp();
    return 0;
}

int PLog::CleanUp()
{
    llist_node *queue = llist_del_all(&m_request_queue);
    if (queue)
    {
        ProcLog(queue);
    }
    return 0;
}



void * PLog::MainProcHelp(void * arg)
{
    PLog *self = (PLog *) arg;
    self->MainProc();
    return NULL;
}

static
bool is_too_big(std::string const & filename, ssize_t max_size)
{
    struct stat st={0};
    int ret = stat(filename.c_str(), &st);
    if (ret != 0)
    {
        return true;
    }

    off_t len = st.st_size;

    if (len >= max_size)
    {
        return true;
    }
    return false;
}

static
int reopen(std::string const & filename)
{
#define RWRWRW  (S_IRUSR|S_IWUSR|S_IRGRP|S_IWGRP|S_IWOTH|S_IROTH)
    int fd = open(filename.c_str(),  O_APPEND | O_CREAT | O_WRONLY, RWRWRW);
#undef RWRWRW
    return fd;
}


static
int lock_reg(int fd, int cmd, int type, off_t offset, int whence, off_t len) {
    if(fd < 0) {
        return -1;
    }
    struct flock lock;
    lock.l_type = type;
    lock.l_start = offset;
    lock.l_whence = whence;
    lock.l_len = len;
    return fcntl(fd, cmd, &lock);
}

static
int shift_file(std::string const &basename, int num)
{
    char log_file[1024];
    snprintf(log_file, sizeof(log_file)-1, "%s.%d", basename.c_str(), num-1);
    int ret = 0;
    if (remove(log_file) < 0 ) {
        ret =  -__LINE__;
        return ret;
    }
    char log_file_old[1024];
    char *src=log_file_old;
    char *dst=log_file;
    char *tmp = NULL;
    for (int i = num - 2; i >= 1; i--) {
        snprintf(src, sizeof(log_file) -1, "%s.%d", basename.c_str(), i);
        if (rename(src,dst) < 0 ) {
            return -__LINE__;
        }
        tmp = dst;
        dst = src;
        src = tmp;
    }
    ret = rename(basename.c_str(), dst);
    return ret;
}

int PLog::check_log()
{
    m_prev_check_timestamp = get_sys_ms();
    if (m_log_type == ROTATE_LOG)
    {
        return check_rotate_log();
    }

    return 0;
}

int PLog::check_rotate_log()
{
    if (is_too_big(m_file_name_prefix, (ssize_t)m_max_file_size))
    {
        manage_rotate_log();
        close(m_fd);
        m_fd = reopen(m_file_name_prefix);
        if (m_fd < 0)
        {
            return -1;
        }
    }
    return 0;
}

int PLog::manage_rotate_log()
{
    int ret = 0;
    int fd = open(m_lock_file.c_str(), O_CREAT|O_WRONLY, 0666);
    if (fd<0)
    {
        return -1;
    }

    if (lock_reg(fd, F_SETLK, F_WRLCK, 0, SEEK_SET, 0) >= 0)
    {
        ret = shift_file(m_file_name_prefix, m_max_file_num);
        if (ret < 0)
        {
            close(fd);
            return -1;
        }
        ret = lock_reg(fd, F_SETLK, F_UNLCK, 0, SEEK_SET, 0);
        if (ret < 0)
        {
            close(fd);
            return -1;
        }
    }
    close(fd);
    return 0;
}

int PLog::InitRotateLog(std::string const & file_name_prefix,
        int max_file_num, int max_file_size)
{
    m_file_name_prefix = file_name_prefix;
    m_max_file_num = max_file_num;
    m_max_file_size = max_file_size;
    m_log_type = ROTATE_LOG;
    m_lock_file = m_file_name_prefix + ".lock";

    m_fd = reopen(m_file_name_prefix);
    if (m_fd < 0)
    {
        return -1;
    }
    int ret = 0;
    ret = check_rotate_log();
    if (ret != 0)
    {
        return -1;
    }
    return 0;
}

int PLog::SetLogLevel(int level)
{
    m_log_level = level;
    return 0;
}

int PLog::SetLogLevel(std::string const &level)
{
    return SetLogLevel(log_level_cast(level));
}

PLog::color_cb_t * PLog::GetColorCb()
{
    if (m_color_key == OUT_OF_INDEXES || !m_color_key) {
        return NULL;
    }

    color_cb_t *ptr =NULL;

    ptr = (color_cb_t *) pthread_getspecific(m_color_key);
    return ptr;
}

void PLog::SetColor(std::string (*cb)(void *arg), void *arg)
{
    if (m_color_key == OUT_OF_INDEXES || !m_color_key) {
        return;
    }

    color_cb_t *ptr = (color_cb_t *) pthread_getspecific(m_color_key);

    if (ptr == NULL)
    {
        ptr = new color_cb_t();
        ptr->cb = NULL;
        ptr->arg = NULL;
        ptr->link_to.next = NULL;
        llist_add(&ptr->link_to, &m_color_all);
        pthread_setspecific(m_color_key, ptr);
    }
    if (ptr) {
        ptr->cb = cb;
        ptr->arg = arg;
    } 
}

void PLog::ClearColor()
{
    color_cb_t *color = GetColorCb();
    if (color) {
        color->cb = NULL;
        color->arg = NULL;
    }
}

}
