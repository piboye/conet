#include "../module.h"
#include "../plog.h"

int main(int argc, char *argv[])
{
    //conet::PLog::Instance().Start();
    InitAllModule(&argc, &argv);

    FinAllModule();
    //conet::PLog::Instance().Stop();
    return 0;
}
