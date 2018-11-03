#include "debug.h"
extern "C"
{
	#include "../common/debug_out.h"
}

debug_out dbg;

debug_out& debug_out::operator<< (char c)
{
	put_char(c);
	return *this;
}

debug_out& debug_out::operator<< (const char* s)
{
	putstring(s);
	return *this;
}

debug_out& debug_out::operator<< (uint8_t b)
{
	put_hex_byte(b);
	return *this;
}

debug_out& debug_out::operator<< (uint16_t s)
{
	put_hex_short(s);
	return *this;
}

debug_out& debug_out::operator<< (uint32_t i)
{
	put_hex_int(i);
	return *this;
}

debug_out& debug_out::operator<< (uint64_t l)
{
	put_hex_long(l);
	return *this;
}

debug_out& debug_out::operator<< (long int l)
{
	put_hex_long(l);
	return *this;
}

debug_out& debug_out::operator<< (long long int ll)
{
	put_hex_long(ll);
	return *this;
}

debug_out& debug_out::operator<< (long long unsigned int llu)
{
	put_hex_long(llu);
	return *this;
}

void die()
{
#if 0
	asm volatile("movl $0, %eax");
	asm volatile("out %eax, $0xf4");
	asm volatile("cli");
#endif
	for (;;)
	{
		asm volatile("hlt");
	}
}

void panic(const char* msg)
{
	dbg << "KERNEL PANIC: " << msg << "\nHalted.";;
	die();
}

