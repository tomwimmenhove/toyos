#include "debug_out.h"

void put_char(unsigned char ch)
{
        asm volatile(   "mov %0, %%al\n"
                        "out %%al, $0xe9"
                        : 
                        :"r" (ch)
                        :"%al");
}

void putstring(const char* s)
{
        while (*s)
        {
                put_char(*s++);
        }
}

void put_hex_nibble(uint8_t b)
{
        static const char* digits = "0123456789abcdef";
        put_char(digits[b & 0xf]);
}

void put_hex_byte(uint8_t b)
{
        put_hex_nibble(b >> 4);
        put_hex_nibble(b);
}

void put_hex_short(uint16_t s)
{
        put_hex_byte(s >> 8);
        put_hex_byte(s);
}

void put_hex_int(uint32_t l)
{
        put_hex_short(l >> 16);
        put_hex_short(l);
}

void put_hex_long(uint64_t ll)
{
        put_hex_int(ll >> 32);
        put_hex_int(ll);
}

