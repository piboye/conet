#ifndef BOBHASH_H
#define BOBHASH_H

#include <stdint.h>     /* defines uint32_t etc */ 
uint32_t bob_hash( const void *key, size_t length, uint32_t initval);

#endif
