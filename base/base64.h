#ifndef BASE64_H_VXIK8QOG
#define BASE64_H_VXIK8QOG

#include <stdint.h>
#include <unistd.h>

namespace conet
{
int base64_encode(char const * str, size_t len, char * dest);
int base64_decode(char const * str, size_t len, char * dest);
}

#endif /* end of include guard: BASE64_H_VXIK8QOG */
