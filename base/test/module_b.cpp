#include "../module.h"
#include "../plog.h"

DEFINE_MODULE(b)
{
    PLOG_INFO("init module b");
    return 0;
}

MODULE_FIN()
{
    PLOG_INFO("fin module b");
    return 0;
}
