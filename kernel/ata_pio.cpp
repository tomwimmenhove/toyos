#include "ata_pio.h"
#include "debug.h"

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

	/* One request at a time */
	req_sem.inc();
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

void ata_pio::read(void* buffer, uint64_t blk_first, int blk_cnt)
{
	//con << "waiting for req_sem\n";
	req_sem.dec();
	//con << "req_sem returnde\n";
	assert(!busy); /* Make sure we're not already processing stuff */
	busy = true;

	out_lba_48(blk_first, blk_cnt);

	outb(0x24, io_addr + io_cmd);

	//con << "waiting for irq_sem\n";
	irq_sem.dec();
	//con << "irq_sem returned\n";

	read_data(buffer, blk_cnt);

	//con << "waking up req_sem";
	req_sem.inc();
}

void ata_pio::read_data(void* buffer, int blk_cnt)
{
	uint16_t* buf = (uint16_t*) buffer;

	for (int i = 0; i < 256 * blk_cnt; i++)
		buf[i] = inw(io_addr + io_data);
}

void ata_pio::write(void* buffer, uint64_t blk_first, int blk_cnt)
{
	req_sem.dec();
	assert(!busy); /* Make sure we're not already processing stuff */
	busy = true;

	out_lba_48(blk_first, blk_cnt);

	outb(0x34, io_addr + io_cmd);

	write_data(buffer, blk_cnt);

	irq_sem.dec();
	req_sem.inc();
}

void ata_pio::write_data(void* buffer, int blk_cnt)
{
	uint16_t* buf = (uint16_t*) buffer;

	for (int i = 0; i < 256 * blk_cnt; i++)
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
	/* XXX: check if iterrupt came from us! */

	/* Read status reg clears interrupt flag */
	inb(io_addr + io_status);

	busy = false;
	//con << "waking up irq_sem";
	irq_sem.inc();
}

