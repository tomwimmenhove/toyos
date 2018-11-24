#include "ata_pio.h"

ata_pio::ata_pio(uint16_t io_addr, uint16_t io_alt_addr, uint8_t irq)
	: intr_driver(irq), io_addr(io_addr), io_alt_addr(io_alt_addr)
{   
	select(false, true);

	/* Reset */
	outb(6, io_alt_addr + io_alt_ctrl);
	outb(2, io_alt_addr + io_alt_ctrl);
	wait_busy(); /* Do we need to wait? */

	/* Unmask interrupts */
	outb(0, io_alt_addr + io_alt_ctrl);
}

void ata_pio::select(bool slave, bool force)
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

void ata_pio::read(void* buffer, uint64_t blk_first, int blk_cnt, embxx::util::StaticFunction<void()> calblk_firstck)
{
	assert(buf == nullptr); /* Make sure we're not already processing stuff */

	read_cmd = true;
	n_sects = blk_cnt;
	buf = (uint16_t*) buffer;
	cb = calblk_firstck;
	
	out_lba_48(blk_first, blk_cnt);

	outb(0x24, io_addr + io_cmd);
}

void ata_pio::write(void* buffer, uint64_t blk_first, int blk_cnt, embxx::util::StaticFunction<void()> calblk_firstck)
{
	assert(buf == nullptr); /* Make sure we're not already processing stuff */

	read_cmd = false;
	n_sects = blk_cnt;
	buf = (uint16_t*) buffer;
	cb = calblk_firstck;

	out_lba_48(blk_first, blk_cnt);

	outb(0x34, io_addr + io_cmd);

	/* Write buffer */
	for (int i = 0; i < 256 * n_sects; i++)
		outw_p(buf[i], io_addr + io_data);
}

void ata_pio::out_lba_48(uint64_t lba, int cnt)
{
	assert(cnt > 0 && cnt <= 65536);
	if (cnt == 65536)
		cnt = 0;

	outb(cnt >> 8,				io_addr + io_sect_cnt);
	outb((lba >> 24) & 0xff,	io_addr + io_sect_num);
	outb((lba >> 32) & 0xff,	io_addr + io_cyl_low);
	outb((lba >> 40) & 0xff,	io_addr + io_cyl_high);

	outb(cnt & 0xff,			io_addr + io_sect_cnt);
	outb(lba & 0xff,			io_addr + io_sect_num);
	outb((lba >> 8) & 0xff,		io_addr + io_cyl_low);
	outb((lba >> 16) & 0xff,	io_addr + io_cyl_high);
}

void ata_pio::interrupt(uint64_t, interrupt_state*)
{   
	if (read_cmd)
	{   
		for (int i = 0; i < 256 * n_sects; i++)
			buf[i] = inw(io_addr + io_data);
	}

	cb();

	buf = nullptr;
}

