#ifndef DEBUG_H
#define DEBUG_H

#include <stdint.h>

class debug_out
{
public: 
	debug_out& operator<< (char c);
	debug_out& operator<< (const char* s);
	debug_out& operator<< (uint8_t b);
	debug_out& operator<< (uint16_t s);
	debug_out& operator<< (uint32_t i);
	debug_out& operator<< (uint64_t l);

	debug_out& operator<< (long int lu);
	debug_out& operator<< (long long int llu);

	debug_out& operator<< (long long unsigned int llu);
};

void die() __attribute__ ((noreturn));
void panic(const char* msg) __attribute__ ((noreturn));

extern debug_out dbg;

#endif /* DEBUG_H */
