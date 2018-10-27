
#ifndef DEBUG_OUT_H
#define DEBUG_OUT_H

#include <stdint.h>

void put_char(unsigned char ch);
void putstring(const char* s);
void put_hex_nibble(uint8_t b);
void put_hex_byte(uint8_t b);
void put_hex_short(uint16_t s);
void put_hex_int(uint32_t l);
void put_hex_long(uint64_t ll);

#endif /* DEBUG_OUT_H */
