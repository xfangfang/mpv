#ifndef _MEMMAP_H_
#define _MEMMAP_H_

#include <stdio.h>
#include <stdint.h>
#include <stdlib.h>

#define PROT_READ       0x1
#define PROT_WRITE      0x2
#define MAP_SHARED      0x01
#define MAP_FAILED      ((void *) -1)

#define mmap(a, b, c, d, e, f) malloc(b)
#define munmap(a, b) free(a)

#endif
