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

struct ata_request
{   
	enum class type
	{   
		read,
		write
	};

	type t;
	void* buf;
	uint64_t sect_start;
	uint64_t sect_cnt;
	bool slave;
	embxx::util::StaticFunction<void()> callback;
};

class ata_pio : public intr_driver
{
public:
	ata_pio(uint16_t io_addr, uint16_t io_alt_addr, uint8_t irq);

	void handle_request(ata_request& req);
	void read_48(void* buffer, uint64_t lba, int sect_cnt, embxx::util::StaticFunction<void()> callback);
	void write_48(void* buffer, uint64_t lba, int sect_cnt, embxx::util::StaticFunction<void()> callback);
	void interrupt(uint64_t, interrupt_state*) override;

private:
	void wait_busy();
	void dev_select(bool slave, bool force = false);

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

	bool read_cmd = false;
	uint16_t n_sects;
	uint16_t* buf;
	embxx::util::StaticFunction<void()> cb;

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


#endif /* ATA_PIO_H */
