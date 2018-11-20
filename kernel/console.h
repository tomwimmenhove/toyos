#ifndef CONSOLE_H
#define CONSOLE_H

#include <stdint.h>
#include <stddef.h>

#include "config.h"

struct console;

struct formatter
{
	virtual void print(console* c) = 0;
};

struct console
{
	virtual void putc(char ch) = 0;
	virtual void write_string(const char* s) = 0;
	virtual void write_buf(const char* s, size_t n) = 0;

	console& operator<< (const char* s);
	console& operator<< (formatter& f);

	console& operator<< (char c);
	console& operator<< (uint8_t val);
	console& operator<< (uint16_t val);
	console& operator<< (uint32_t val);
	console& operator<< (uint64_t val);
	console& operator<< (int val);
	console& operator<< (long int val);
	console& operator<< (long long int val);
	console& operator<< (long long unsigned int val);

protected:
	void pr_int_s(long long int val);
	void pr_int_u(unsigned long long int val);

private:
	void pr_int(unsigned long long int val, unsigned long long int div);
};

template<class T>
struct hex : public formatter
{
	inline void print(console* c) override
	{
		static const char digits[] = "0123456789abcdef";
		for (int i = sizeof(T) - 1; i >= 0; i--)
		{   
			uint8_t b = value >> (i * 8);
			c->putc(digits[b >> 4]);
			c->putc(digits[b & 0xf]);
		}
	}

	inline formatter& operator() (T v) { value = v; return *this; }

private:
	T value;
};

extern hex<uint8_t> hex_u8;
extern hex<uint16_t> hex_u16;
extern hex<uint32_t> hex_u32;
extern hex<uint64_t> hex_u64;

struct console_x86 : public console
{
	console_x86(uint8_t* base, int w, int h)
		: base(base), w(w), h(h), pos(0)
	{ }

	void init(kernel_boot_info* kbi);

	void putc(char ch) override;
	void write_string(const char* s) override;
	void write_buf(const char* s, size_t n) override;

	uint8_t* base;
	int w;
	int h;

private:
	static uint16_t read_pos();
	static void write_pos(uint16_t p);

	int pos;
};

extern console_x86 con;

struct console_user : public console
{
	console_user(int fd) : fd(fd)
	{ }

	void putc(char ch) override;
	void write_string(const char* s) override;
	void write_buf(const char* s, size_t n) override;

	int fd;
};

extern console_user ucon;

#endif /* CONSOLE_H */
