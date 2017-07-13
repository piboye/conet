#include "../module.h"
#include "../plog.h"

DEFINE_MODULE(a)
{
    PLOG_INFO("init module a");
    return 0;
}

MODULE_FIN()
{
    PLOG_INFO("fin module a");
    return 0;
}

USING_MODULE(b);

