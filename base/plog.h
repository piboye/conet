#ifndef PLOG_H_LKXU67QB
#define PLOG_H_LKXU67QB
#include "llist.h"
#include <string>
#include <pthread.h>
#include <stdint.h>
#include "log_format.h"
#include "time_helper.h"
#include <sys/types.h>

class PLog
{
public:
    enum
    {
        DEBUG = 1,
        INFO = 2,
        ERROR = 3,
    };

    struct ReqItem
    {
        char const * file_name;
        char const * func;
        int line;
        uint64_t thread_id;
        uint64_t timestamp;
        std::string text;
        llist_node link_to;
        int level;
    };

    llist_head m_request_queue;
    int64_t m_queue_len;

    int32_t m_initialized;
    int m_work_notify;

    pthread_t m_main_thread;
    int m_stop_flags;
    int m_fd;

    enum
    {
        STDERR_LOG = 0,
        ROTATE_LOG = 1,
    };

    int m_log_type;
    std::string m_file_name_prefix;
    std::string m_lock_file;
    int m_max_file_num;
    int m_max_file_size;
    int64_t m_prev_check_timestamp;

    int Add(ReqItem *item);

    static PLog & Instance();
    PLog();
    ~PLog();

    int InitRotateLog(std::string const & file_name_prefix,
            int max_file_num, int max_file_size);

    int Start();
    int Stop();

    int ProcLog(llist_node *queue);

//private:
    int MainProc();
    static void * MainProcHelp(void *);
    int CleanUp();

    int check_log();
    int check_rotate_log();
    int manage_rotate_log();
};

#define PLOG(a_level, ...) \
do  \
{ \
    PLog::ReqItem *item = new PLog::ReqItem(); \
    item->level = PLog::a_level; \
    item->file_name = __FILE__; \
    item->func = __FUNCTION__; \
    item->line = __LINE__; \
    item->timestamp = conet::get_sys_ms(); \
    LOG_FORMAT(item->text, #__VA_ARGS__); \
    PLog::Instance().Add(item); \
} while(0)



#endif /* end of include guard: PLOG_H_LKXU67QB */
