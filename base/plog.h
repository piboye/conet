#ifndef PLOG_H_LKXU67QB
#define PLOG_H_LKXU67QB
#include "llist.h"
#include <string>
#include <pthread.h>
#include <stdint.h>
#include "log_format.h"
#include "time_helper.h"
#include <sys/types.h>
#include <unistd.h>
#include <sys/time.h>
#include "gflags/gflags.h"
#include "list.h"

DECLARE_int32(plog_type);
DECLARE_string(plog_file);
DECLARE_int32(plog_max_file_num);
DECLARE_int32(plog_max_file_size);
DECLARE_string(plog_level);

namespace conet
{
class PLog
{
public:
    enum
    {
        DEBUG = 1,
        INFO = 2,
        WARN = 3,
        ERROR = 4,
        ALERT = 5,
        FATAL = 6,
    };

    struct ReqItem
    {
        char const * file_name;
        char const * func;
        int line;
        uint64_t thread_id;
        struct timeval timestamp;
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
    int m_log_level;

    // 染色相关
    pthread_key_t m_color_key;

    struct color_cb_t
    {
        std::string (*cb)(void *arg);
        void *arg;
        llist_node link_to;
    };

    llist_head m_color_all;

    color_cb_t * GetColorCb();
    void SetColor(std::string (*cb)(void *arg), void *arg);
    void ClearColor();

    int Add(ReqItem *item);

    int AddFast(ReqItem *item);

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

    int SetLogLevel(int level);
    int SetLogLevel(std::string const &level);

    int check_log();
    int check_rotate_log();
    int manage_rotate_log();
};


// C 格式输出
#define PLOG_RAW(a_level, fmt, ...) \
do  \
{  \
    if (conet::PLog::a_level < conet::PLog::Instance().m_log_level) break; \
    conet::PLog::ReqItem *__plog_item__ = new conet::PLog::ReqItem(); \
    __plog_item__->level = conet::PLog::a_level; \
    __plog_item__->file_name = __FILE__; \
    __plog_item__->func = __FUNCTION__; \
    __plog_item__->line = __LINE__; \
    __plog_item__->text.resize(1024); \
    size_t __plog_len = 1024; \
    __plog_len = snprintf((char *)__plog_item__->text.data(), __plog_item__->text.size()-1, fmt "\n", ##__VA_ARGS__); \
    if (__plog_len > __plog_item__->text.size()-1) { \
        __plog_item__->text.resize(__plog_len+1); \
        __plog_len = snprintf((char *)__plog_item__->text.data(), __plog_item__->text.size()-1, \
                fmt, ##__VA_ARGS__); \
    } \
    __plog_item__->text.resize(__plog_len); \
    conet::PLog::color_cb_t * __plog_color_cb = conet::PLog::Instance().GetColorCb();  \
    if (__plog_color_cb)  \
    {  \
       if (__plog_color_cb->cb) \
            __plog_item__->text.append(__plog_color_cb->cb(__plog_color_cb->arg)); \
    } \
    __plog_item__->text.push_back('\n'); \
    if (conet::PLog::FATAL == conet::PLog::a_level) { \
        conet::PLog::Instance().AddFast(__plog_item__); \
        abort();\
    } else if (conet::PLog::Instance().Add(__plog_item__) != 0) delete __plog_item__; \
} while(0)

// 扩展格式   (a, b, c)
//
#define PLOG(a_level, ...) \
do  \
{  \
    if (conet::PLog::a_level < conet::PLog::Instance().m_log_level) break; \
    conet::PLog::ReqItem *__plog_item__ = new conet::PLog::ReqItem(); \
    __plog_item__->level = conet::PLog::a_level; \
    __plog_item__->file_name = __FILE__; \
    __plog_item__->func = __FUNCTION__; \
    __plog_item__->line = __LINE__; \
    LOG_FORMAT(__plog_item__->text, ##__VA_ARGS__); \
    conet::PLog::color_cb_t * __plog_color_cb = conet::PLog::Instance().GetColorCb();  \
    if (__plog_color_cb)  \
    {  \
       if (__plog_color_cb->cb) \
        __plog_item__->text.append(__plog_color_cb->cb(__plog_color_cb->arg)); \
    } \
    __plog_item__->text.push_back('\n'); \
    if (conet::PLog::FATAL == conet::PLog::a_level) { \
        conet::PLog::Instance().AddFast(__plog_item__); \
        abort();\
    } else if (conet::PLog::Instance().Add(__plog_item__) != 0) delete __plog_item__; \
} while(0)

/*
#define PLOG_DEBUG(...) PLOG(DEBUG, ##__VA_ARGS__)
#define PLOG_INFO(...)  PLOG(INFO, ##__VA_ARGS__)
#define PLOG_WARN(...)  PLOG(WARN, ##__VA_ARGS__)
#define PLOG_ERROR(...) PLOG(ERROR, ##__VA_ARGS__)
#define PLOG_ALERT(...) PLOG(ALERT, ##__VA_ARGS__)
#define PLOG_FATAL(...) PLOG(FATAL, ##__VA_ARGS__)
*/


#define PLOG_DEBUG(...)
#define PLOG_INFO(...)
#define PLOG_WARN(...)
#define PLOG_ERROR(...)
#define PLOG_ALERT(...)
#define PLOG_FATAL(...)

}



#endif /* end of include guard: PLOG_H_LKXU67QB */
