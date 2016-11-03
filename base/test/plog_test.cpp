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

    PLOG(INFO, "hello");
    PLOG(INFO, "world");

    PLog::Instance().Stop();
    return 0;
}
