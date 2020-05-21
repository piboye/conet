#include "../plog.h"
#include <iostream>
#include <time.h>
#include "module.h"

int main(int argc, char **argv)
{
    InitAllModule(argc, argv);
    PLOG_ERROR("wellcome");
    PLOG(DEBUG, "ignore this msg");
    PLOG(INFO, "hello ", "world");
    PLOG(INFO, "my age: ", 18);
    PLOG_RAW(INFO, "my age: %d", 18);

    return 0;
}
