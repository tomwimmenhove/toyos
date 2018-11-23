#include "ata_pio.h"

ata_pio::ata_pio(uint16_t io_addr, uint16_t io_alt_addr, uint8_t irq)
	: intr_driver(irq), io_addr(io_addr), io_alt_addr(io_alt_addr)
{   
	dev_select(false, true);

	/* Reset */
	outb(6, io_alt_addr + io_alt_ctrl);
	outb(2, io_alt_addr + io_alt_ctrl);
	wait_busy(); /* Do we need to wait? */

	/* Unmask interrupts */
	outb(0, io_alt_addr + io_alt_ctrl);
}

void ata_pio::handle_request(ata_request& req)
{   
	dev_select(req.slave);

	switch(req.t)
	{   
		case ata_request::type::read:
			read_48(req.buf, req.sect_start, req.sect_cnt, req.callback);
			break;
		case ata_request::type::write:
			write_48(req.buf, req.sect_start, req.sect_cnt, req.callback);
			break;
	}
}

void ata_pio::dev_select(bool slave, bool force)
{
	if (!force && slave == slave_active)
		return;

	outb((uint8_t) drive_reg::always | (uint8_t) drive_reg::lba | (slave ? (uint8_t) drive_reg::slave : 0), io_addr + io_drive);

	wait_busy();
}

void ata_pio::wait_busy()
{
	inb(io_alt_addr + io_alt_status);
	inb(io_alt_addr + io_alt_status);
	inb(io_alt_addr + io_alt_status);
	inb(io_alt_addr + io_alt_status);

	for (;;)
		if ((inb(io_alt_addr + io_alt_status) & (uint8_t) status_reg::bsy) == 0)
			break;
}

void ata_pio::read_48(void* buffer, uint64_t lba, int sect_cnt, embxx::util::StaticFunction<void()> callback)
{
	assert(buf == nullptr); /* Make sure we're not already processing stuff */
	assert(sect_cnt > 0 && sect_cnt <= 65536);
	if (sect_cnt == 65536)
		sect_cnt = 0;

	read_cmd = true;
	n_sects = sect_cnt;
	buf = (uint16_t*) buffer;
	cb = callback;
	outb(sect_cnt >> 8,         io_addr + io_sect_cnt);
	outb((lba >> 24) & 0xff,    io_addr + io_sect_num);
	outb((lba >> 32) & 0xff,    io_addr + io_cyl_low);
	outb((lba >> 40) & 0xff,    io_addr + io_cyl_high);

	outb(sect_cnt & 0xff,       io_addr + io_sect_cnt);
	outb(lba & 0xff,            io_addr + io_sect_num);
	outb((lba >> 8) & 0xff,     io_addr + io_cyl_low);
	outb((lba >> 16) & 0xff,    io_addr + io_cyl_high);

	outb(0x24, io_addr + io_cmd);
}

void ata_pio::write_48(void* buffer, uint64_t lba, int sect_cnt, embxx::util::StaticFunction<void()> callback)
{
	assert(buf == nullptr); /* Make sure we're not already processing stuff */
	assert(sect_cnt > 0 && sect_cnt <= 65536);
	if (sect_cnt == 65536)
		sect_cnt = 0;

	read_cmd = true;
	n_sects = sect_cnt;
	buf = (uint16_t*) buffer;
	cb = callback;

	outb(sect_cnt >> 8,         io_addr + io_sect_cnt);
	outb((lba >> 24) & 0xff,    io_addr + io_sect_num);
	outb((lba >> 32) & 0xff,    io_addr + io_cyl_low);
	outb((lba >> 40) & 0xff,    io_addr + io_cyl_high);

	outb(sect_cnt & 0xff,       io_addr + io_sect_cnt);
	outb(lba & 0xff,            io_addr + io_sect_num);
	outb((lba >> 8) & 0xff,     io_addr + io_cyl_low);
	outb((lba >> 16) & 0xff,    io_addr + io_cyl_high);

	outb(0x34, io_addr + io_cmd);

	/* Write buffer */
	for (int i = 0; i < 256 * n_sects; i++)
		outw_p(buf[i], io_addr + io_data);

}

void ata_pio::interrupt(uint64_t, interrupt_state*)
{   
	if (read_cmd)
	{   
		for (int i = 0; i < 256 * n_sects; i++)
			buf[i] = inw(io_addr + io_data);
		cb();
	}

	buf = nullptr;
}

