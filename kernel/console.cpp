#include "console.h"
#include "io.h"
#include "debug.h"

console_x86 con((uint8_t*) (0xffffffff40000000 - 0x1000), 80, 25);

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


void console_x86::write_string(const char* s)
{

//	outb(0x3D4, 0x0F);
//	outb(0x3D5, (uint8_t) (pos & 0xFF));
//	outb(0x3D4, 0x0E);
//	outb(0x3D5, (uint8_t) ((pos >> 8) & 0xFF));

	outb(0x0f, 0x3d4);
	uint16_t p = inb(0x3d5);
//	outb(0x3D5, (uint8_t) (pos & 0xFF));
	outb(0x0e, 0x3d4);
	p |= inb(0x3d5) << 8;
//	outb(0x3D5, (uint8_t) ((pos >> 8) & 0xFF));

	dbg << "p: " << p << '\n';

	char ch;
	while ((ch = *s++))
	{
		if (ch == '\n')
			/* new line */
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

			dbg << "pos: " << (uint16_t) pos << '\n';

		}

		if (ch == '\n')
		{
			write_pos(pos);
			continue;
		}

		base[pos * 2] = ch;

		pos++;
		write_pos(pos);
	}
}
