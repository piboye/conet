#include "./timeout_mgr.h"
#include <stdio.h>

TimeoutManager g_tmgr;

void f(void *)
{

}

int main(int argc, char const *argv[])
{
	if (argc < 2)
	{
        fprintf(stderr, "usage:%s num\n", argv[0]); 		
        return 0;
	}

	int num = atoi(argv[1]);
	new timeout_handle_t();

	return 0;
}
