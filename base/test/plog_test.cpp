#include "../plog.h"
#include <iostream>
#include <time.h>
#include "module.h"

std::string log_color(void *)
{
    return " log color ";
}

int main(int argc, char **argv)
{
    InitAllModule(argc, argv);
    conet::PLog::Instance().SetColor(&log_color, NULL);

    PLOG_ERROR("wellcome");
    PLOG(DEBUG, "ignore this msg");
    PLOG(INFO, "hello ", "world");
    PLOG(INFO, "my age: ", 18);
    PLOG_RAW(INFO, "my age: %d", 18);

    return 0;
}
