#include "thirdparty/gflags/gflags.h"
#include "../plog.h"
#include "../delay_init.h"
#include <iostream>

int main(int argc, char **argv)
{
    int ret = 0;
    ret = gflags::ParseCommandLineFlags(&argc, &argv, false);
    ret = delay_init::call_all_level();
    if (ret != 0)
    {
        std::cerr<<"init all module failed";
        return 0;
    }

    PLOG_ERROR("wellcome");
    PLOG(DEBUG, "ignore this msg");
    PLOG(INFO, "hello ", "world");
    PLOG(INFO, "my age: ", 18);
    PLOG_RAW(INFO, "my age: %d", 18);

    PLog::Instance().Stop();
    return 0;
}
