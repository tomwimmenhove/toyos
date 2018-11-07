#ifndef CONSOLE_H
#define CONSOLE_H

#include <stdint.h>

struct console
{
	virtual void write_string(const char* s) = 0;
};

struct console_x86 : public console
{
	console_x86(uint8_t* base, int w, int h)
		: base(base), w(w), h(h), pos(0)
	{ }

	void init();

	void write_string(const char* s) override;

	static uint16_t read_pos();
	static void write_pos(uint16_t p);

	uint8_t* base;
	int w;
	int h;
	int pos;
};

extern console_x86 con;

#endif /* CONSOLE_H */
