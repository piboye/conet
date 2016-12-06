#ifndef ROTATE_LOG_H_1CVR0YSZ
#define ROTATE_LOG_H_1CVR0YSZ

#include <unistd.h>
#include <stdio.h>
#include <sstream>
#include <sys/types.h>
#include <sys/stat.h>
#include <fcntl.h>
#include <assert.h>
#include <strings.h>

class RotateLog
{
public:
    enum
    {
        DEBUG=0,
        INFO,
        WARN,
        ERROR,
        ALERT,
    };

    int fd_;

    static int log_level_cast(std::string  const & a_str)
    {
        char const * str  = a_str.c_str();
        if (NULL ==str) str = "";
#define LOG_LEVEL_STR_CAST(level) if (strcasecmp(str, #level) == 0) return level
        LOG_LEVEL_STR_CAST(DEBUG);
        LOG_LEVEL_STR_CAST(INFO);
        LOG_LEVEL_STR_CAST(WARN);
        LOG_LEVEL_STR_CAST(ERROR);
        LOG_LEVEL_STR_CAST(ALERT);
#undef LOG_LEVEL_STR_CAST
        return DEBUG;
    }

    ~RotateLog()
    {
        RotateLog::close();
    }

    FILE *get_file() {
        if (NULL == file_) {
            reopen();
        }
        return file_;
    }


    int init(char const* basename, int file_size, int file_num, int level = RotateLog::DEBUG) {
        basename_ = basename;
        file_size_ = file_size;
        file_num_ = file_num;
        lock_file_ = basename;
        lock_file_ +=".lock";
        level_ = level;
        file_ = NULL;
        fd_ = -1;
        return 0; 
    }

    int set_level(int level) {
        assert(level>=0);
        level_ = level;
        return level_;
    }

    int get_level() const {
        return level_;
    }

    int close() {
        if (file_) {
            fclose(file_);
            file_=NULL;
        }
        return 0;
    }

    int reopen() {
        this->close();
#define RWRWRW  (S_IRUSR|S_IWUSR|S_IRGRP|S_IWGRP|S_IWOTH|S_IROTH)
        mode_t old_mask = umask(0);
        FILE *file = fopen(basename_, "a"); 
        umask(old_mask);
#undef RWRWRW

        if (file == NULL) {
            return -__LINE__;
        }
        setvbuf(file, NULL, _IONBF, 0);
        file_ = file;
        return 0;
    }

    bool is_too_big() {
        struct stat st;

        int ret = stat(basename_, &st);
        if (ret) {
            return true; 
        } 

        off_t len = st.st_size; 

        if (len >= file_size_) { 
            return true;
        }
        return false;
    }


    int check() {
        if (is_too_big()) {
            manage_log();
            reopen();
        }
        return 0;
    }

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

    int manage_log() {
        int fd = open(lock_file_.c_str(), O_CREAT|O_WRONLY, 0666);
        if (fd<0) { 
            shift_file(basename_, file_num_);
            return 0;
        }

        if (lock_reg(fd, F_SETLK, F_WRLCK, 0, SEEK_SET, 0) >= 0) {
            shift_file(basename_, file_num_);
            lock_reg(fd, F_SETLK, F_UNLCK, 0, SEEK_SET, 0);
        }
        close(fd);
        return 0;
    }



    int shift_file(char const *basename, int num) {
        char log_file[1024];
        snprintf(log_file, sizeof(log_file)-1, "%s.%d", basename, num-1); 
        int ret = 0;
        if (remove(log_file) < 0 ) {
            ret =  -__LINE__;
        }
        char log_file_old[1024];
        char *src=log_file_old;
        char *dst=log_file;
        char *tmp = NULL;
        for (int i = num - 2; i >= 1; i--) {
            snprintf(src, sizeof(log_file) -1, "%s.%d", basename, i);
            if (rename(src,dst) < 0 ) {
                ret = -__LINE__-i*1000000;
            }
            tmp = dst;
            dst = src;
            src = tmp;
        }
        rename(basename, dst);
        return ret;
    }

    int file_num_;
    char const * basename_;
    int file_size_;
    FILE * file_;
    std::string lock_file_;
    int level_;
};

#include <unistd.h>
#include <time.h>

#define ROTATE_LOG(log, level, format, ...) \
    do {\
        if(RotateLog::level>=log.level_ ) { \
            log.check(); \
            char time_buf[100]; \
            time_t now = time(NULL); \
            struct tm * tm= localtime(&now); \
            strftime(time_buf, sizeof(time_buf)-1, "%F %T",tm);   \
            pid_t pid = getpid(); \
            log.reopen();\
            FILE *file = log.get_file(); \
            if (file)  \
            fprintf(file,  "[%s][%s][%d][%s:%d]: " format "\n",  \
                    time_buf, #level, pid, __FILE__, int(__LINE__), ##__VA_ARGS__);\
        }\
    } while(0)


#endif /* end of include guard: ROTATE_LOG_H_1CVR0YSZ */
