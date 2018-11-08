#include "console.h"
#include "io.h"
#include "syscall.h"

/* Generic console stuff */

console& console::operator<< (const char* s)
{
	write_string(s);
	return *this;
}

void console::pr_int(unsigned long long int val, unsigned long long int div)
{
	static char s[20];
	int first_nz = -1;
	int p = 0;
	while (div)
	{
		auto x = val / div;
		if (first_nz == -1 && x != 0)
			first_nz = p;

		s[p++] = x + '0';

		val -= x * div;
		div /= 10;
	}
	s[p] = 0;

	if (first_nz == -1)
	{
		putc('0');
		return;
	}
	write_string(&s[first_nz]);
}

void console::pr_int_s(long long int val)
{
	if (val < 0)
	{
		putc('-');
		pr_int((unsigned long long int) -val, 1000000000000000000llu);
	}
	else
	{
		pr_int((unsigned long long int) val, 1000000000000000000llu);
	}
}

void console::pr_int_u(unsigned long long int val)
{
	pr_int(val, 10000000000000000000llu);
}

console& console::operator<< (char c)
{
	putc(c);
	return *this;
}

console& console::operator<< (uint8_t val)
{
	pr_int_s(val);
	return *this;
}

console& console::operator<< (uint16_t val)
{
	pr_int_s(val);
	return *this;
}

console& console::operator<< (uint32_t val)
{
	pr_int_s(val);
	return *this;
}

console& console::operator<< (uint64_t val)
{
	pr_int_s(val);
	return *this;
}

console& console::operator<< (int val)
{
	pr_int_s(val);
	return *this;
}

console& console::operator<< (long int val)
{
	pr_int_s(val);
	return *this;
}

console& console::operator<< (long long int val)
{
	pr_int_s(val);
	return *this;
}

console& console::operator<< (unsigned long long int val)
{
	pr_int_u(val);
	return *this;
}

console& console::operator<< (formatter& f)
{
	f.print(this);
	return *this;
}

hex<uint8_t> hex_u8;
hex<uint16_t> hex_u16;
hex<uint32_t> hex_u32;
hex<uint64_t> hex_u64;

/* X86 console stuff */

//console_x86 con((uint8_t*) (0xffffffff40000000 - 0x1000), 80, 25);
console_x86 con((uint8_t*) 0xb8000, 80, 25);

uint16_t console_x86::read_pos()
{
	outb(0x0f, 0x3d4);
	uint16_t p = inb(0x3d5);
	outb(0x0e, 0x3d4);
	return p | inb(0x3d5) << 8;
}

void console_x86::write_pos(uint16_t p)
{
	outb(0x0f, 0x3d4);
	outb(p & 0xff, 0x3d5);
	outb(0x0e, 0x3d4);
	outb((p >> 8) & 0xff, 0x3d5);
}

void console_x86::init()
{
	/* enable cursor */
	outb(0x0A, 0x3D4);
	outb((inb(0x3D5) & 0xC0) | 0, 0x3d5);

	outb(0x0B, 0x3d4);
	outb((inb(0x3D5) & 0xE0) | 15, 0x3d5);

	pos = read_pos();
}

void console_x86::putc(char ch)
{
	/* new line? */
	if (ch == '\n')
		pos = (pos + w) - (pos % w);

	if (pos >= w * h)
	{
		/* Scroll */
		for (int i = 0; i < w * h - w; i++)
			base[i * 2] = base[(i + w) * 2];

		/* Blank last line */
		for (int i = 0; i < w; i++)
			base[(i + w * h - w) * 2] = 0;
		pos = w * h - w;
	}

	if (ch != '\n')
	{
		base[pos * 2] = ch;
		pos++;
	}
	write_pos(pos);
}

void console_x86::write_string(const char* s)
{
	char ch;
	while ((ch = *s++))
	{
		putc(ch);
	}
}

/* The 'user' console */
console_user ucon;

void console_user::putc(char ch)
{
	syscall(1, ch);
}

void console_user::write_string(const char* s)
{
	syscall(0, (uint64_t) s);
}
