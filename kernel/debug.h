#ifndef DEBUG_H
#define DEBUG_H

#include <stdint.h>
#include "console.h"

void die() __attribute__ ((noreturn));
void panic(const char* msg) __attribute__ ((noreturn));

#endif /* DEBUG_H */
