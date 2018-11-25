#ifndef ATA_PIO_H
#define ATA_PIO_H

#include <memory>
#include <stdint.h>
#include <stddef.h>
#include <embxx/container/StaticQueue.h>
#include <embxx/util/StaticFunction.h>

#include "io.h"
#include "new.h"
#include "interrupts.h"
#include "semaphore.h"
#include "disk.h"

class ata_pio : public intr_driver
{
public:
	ata_pio(uint16_t io_addr, uint16_t io_alt_addr, uint8_t irq);

	void read(void* buffer, uint64_t blk_first, int blk_cnt);
	void read_data(void* buffer, int blk_cnt);
	void write(void* buffer, uint64_t blk_first, int blk_cnt);
	void write_data(void* buffer, int blk_cnt);
	void interrupt(uint64_t, interrupt_state*) override;
	void select(bool slave, bool force = false);

private:
	void out_lba_48(uint64_t lba, int cnt);
	void wait_busy();

	enum class drive_reg
	{   
		slave = 0x10,
		always = 0x20 | 0x80,
		lba = 0x40,
	};

	enum class status_reg
	{   
		err = 0x01,
		idx = 0x02,
		corr = 0x04,
		drq = 0x08,
		srv = 0x10,
		df = 0x20,
		rdy = 0x40,
		bsy = 0x80,
	};
	uint16_t io_addr;
	uint16_t io_alt_addr;
	bool slave_active = false;

	semaphore irq_sem;
	semaphore req_sem;

	bool busy = false;

	static constexpr int16_t io_data            = 0;
	static constexpr int16_t io_error           = 1;
	static constexpr int16_t io_features        = 1;
	static constexpr int16_t io_sect_cnt        = 2;
	static constexpr int16_t io_sect_num        = 3;
	static constexpr int16_t io_cyl_low         = 4;
	static constexpr int16_t io_cyl_high        = 5;
	static constexpr int16_t io_drive           = 6;
	static constexpr int16_t io_status          = 7;
	static constexpr int16_t io_cmd             = 7;

	static constexpr int16_t io_alt_status      = 0;
	static constexpr int16_t io_alt_ctrl        = 0;
	static constexpr int16_t io_alt_drv_addr    = 1;
};

/*
class ata_disk : public disk_block_io
{
publi:
	ata_disk(std::shared_ptr<ata_pio> ata, bool slave)
		: ata(ata), slave(slave)
	{ }

	void read(void* buffer, uint64_t blk_first, int blk_cnt, embxx::util::StaticFunction<void()> callback) override
	{
		ata->select(slave);
		ata->read(buffer, blk_first, blk_cnt, callback); 
	}

	void write(void* buffer, uint64_t blk_first, int blk_cnt, embxx::util::StaticFunction<void()> callback) override
	{
		ata->select(slave);
		ata->write(buffer, blk_first, blk_cnt, callback); 
	}

private:
	std::shared_ptr<ata_pio> ata;
	bool slave;
};
*/
#endif /* ATA_PIO_H */
