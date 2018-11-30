#include <stdint.h>
#include <stddef.h>
#include <memory>

#include "config.h"
#include "linker.h"
#include "new.h"
#include "mb.h"
#include "mem.h"
#include "malloc.h"
#include "frame_alloc.h"
#include "descriptors.h"
#include "gdt.h"
#include "idt.h"
#include "interrupts.h"
#include "pic.h"
#include "console.h"
#include "syscall.h"
#include "task_helper.h"
#include "task.h"
#include "klib.h"
#include "dev.h" 
#include "syscalls.h"
#include "cache_alloc.h"
#include "kbd.h"
#include "ata_pio.h"
#include "spinlock.h"
#include "semaphore.h"

void print_stack_use()
{
	con << "stack usuage: " << (KERNEL_STACK_TOP - (uint64_t) __builtin_frame_address(0)) << "\n";
}

void dead_task()
{
	panic("Can't handle returning tasks yet");
}

uint64_t jiffies = 0;

std::shared_ptr<task> tasks;
std::shared_ptr<task> current;

std::shared_ptr<task> task_idle;

devices devs;

void test()
{
#if 1
	std::vector<int> tst;
	for(int i = 0; i < 10000000; i++)
		tst.push_back(i);

	for(int i = 0; i < 10000000; i++)
		assert(tst[i] == i);
#endif

	auto drv_kbd = cache_alloc<driver_tty>::take_shared();

	drv_kbd->dev_type = 0;

	devs.add(drv_kbd);
}

struct tst
{
	char data[512];
};

uint8_t ata_buf1[128 * 1024];
extern "C" void k_test_user1(uint64_t arg0, uint64_t arg1)
{
	int fd = open(0, 0);
	console_user ucon(fd);

    ucon << "tsk 1: arg0: " << arg0 << '\n';
	ucon << "tsk 1: arg1: " << arg1 << '\n';

	/* Hacky-as-fuck cd init thing */
	syscall(12);

	/* Open a file on the cd */
	auto cd_fd = open("boot/../bla/othefile.txt;1");
	auto len = read(cd_fd, (void*) ata_buf1, sizeof(ata_buf1));

	ucon.write_buf((const char*) ata_buf1, len);

	//syscall(9, (uint64_t) ata_buf1, 0, 10);

//	syscall(10, (uint64_t) ata_buf, 0, 1);
	
//	for (int i = 0; i < 512; i++)
//		ucon << hex_u8(ata_buf[i]) << "  ";

	uint8_t abuf[512];
	
	syscall(10, (uint64_t) abuf, 0x180, 6);
	ucon << "partial: \"" << (const char*) abuf << "\"\n";

//	syscall(10, (uint64_t) abuf, 0x180, 6);
//	ucon << "partial: \"" << (const char*) abuf << "\"\n";

	/* Test partial write */
	abuf[0] = 42;
	syscall(11, (uint64_t) abuf, 2, 1);

	
	syscall(10, (uint64_t) ata_buf1, 0, 512);

	for (;;)
	{
		syscall(10, (uint64_t) abuf, 0, 512);

		if (memcmp(ata_buf1, abuf, 512) != 0)
		{
			ucon << "1: ATA FUCKED UP\n";
			asm volatile("hlt"); // Cause crash
		}

	//	ucon << "1: " << hex_u8(ata_buf1[511]);
		//ucon << "1";
		//syscall(7);
	}
}

uint8_t ata_buf2[128 * 1024];
void k_test_user2(uint64_t arg0, uint64_t arg1)
{
//	for(;;);
	int fd = open(0, 0);
	console_user ucon(fd);

    ucon << "tsk 2: arg0: " << arg0 << '\n';
	ucon << "tsk 2: arg1: " << arg1 << '\n';

	//uint8_t abuf[512];
	//syscall(10, (uint64_t) abuf, 0x8000, 512);

	char buf[100];
	for (;;)
	{
#if 0
		syscall(10, (uint64_t) ata_buf2, 0x8000, 512);
		if (memcmp(ata_buf2, abuf, 512) != 0)
		{
			ucon << "2: ATA FUCKED UP\n";
			asm volatile("hlt"); // Cause crash
		}

		ucon << "2: " << hex_u8(ata_buf2[1]);

		continue;
#endif
		/* Read syscall:   sysc  fd  buffer          size */
		auto len = read(fd, (void*) buf, sizeof(buf));

		for (size_t i = 0; i < len; i++)
			ucon << buf[i];
//			ucon << '(' << buf[i] << "): " << ((char) buf[i]) << '\n';
	}
}

void task_add(std::shared_ptr<task> task)
{
	auto head = tasks;

	tasks = task;
	task->next = head;
}

void schedule()
{
	auto last = current;

	for(;;)
	{
		current = current->next;
		if (current == nullptr)
			current = tasks;

		if (current->can_run())
			break;

		if (current == last)
		{
			/* Nothing to do? */
			if (!current->can_run())
				current = task_idle;
			break;
		}
	}

	//con << "Switched to task " << current->id << '\n';

	/* Setup new kernel stack */
	tss0.rsp0 = current->tss_rsp;

	/* Switch tasks */
	state_switch(current->rsp, last->rsp);
}

static inline void halt()
{
	asm volatile(
			"sti\n"
			"hlt\n"
			"cli\n");
}

void tsk_idle()
{
	for (;;)
	{
		schedule();
//		ucon << 'X';
		halt();
	}
}

std::shared_ptr<ata_pio> ata0;
std::shared_ptr<disk_block_io> hda;

struct __attribute__((packed)) iso9660_dir_entry
{
	uint8_t len;			//0	1	Length of Directory Record.
	uint8_t ext_attr;		//1	1	Extended Attribute Record length.
	uint32_t extent_le;		//2	8	Location of extent (LBA) in both-endian format.
	uint32_t extent_be;
	uint32_t data_len_le;	//10	8	Data length (size of extent) in both-endian format.
	uint32_t data_len_be;
	uint8_t rec_date[7];	//18	7	Recording date and time (see format below).
	uint8_t file_flags;		//25	1	File flags (see below).
	uint8_t file_usize;		//26	1	File unit size for files recorded in interleaved mode, zero otherwise.
	uint8_t gap_size;		//27	1	Interleave gap size for files recorded in interleaved mode, zero otherwise.
	uint16_t vol_seq_le;	//28	4	Volume sequence number - the volume that this extent is recorded on, in 16 bit both-endian format.
	uint16_t vol_seq_be;
	uint8_t file_id_len;	//32	1	Length of file identifier (file name). This terminates with a ';' character followed by the file ID number in ASCII coded decimal ('1').
	char file_id[222];		//33	(variable)	File identifier.
};

struct __attribute__((packed)) i_iso9660_vol_desc { };

struct __attribute__((packed)) iso9660_vol_desc : public i_iso9660_vol_desc
{
	uint8_t type;
	char ident[5];
	uint8_t version;
	uint8_t data[2041];
};

struct __attribute__((packed)) iso9660_vol_desc_primary : public i_iso9660_vol_desc
{
	uint8_t type;
	char ident[5];
	uint8_t version;
	uint8_t unused0;			//7	1	Unused	-	Always 0x00.
	char sys_id[32];			//8	32	System Identifier	strA	The name of the system that can act upon sectors 0x00-0x0F for the volume.
	char vol_id[32];			//40	32	Volume Identifier	strD	Identification of this volume.
	uint8_t zeroes0[8];			//72	8	Unused Field	-	All zeroes.
	uint32_t vol_space_le;		//80	8	Volume Space Size	int32_LSB-MSB	Number of Logical Blocks in which the volume is recorded.
	uint32_t vol_space_be;		//
	uint8_t zeroes1[32];		//88	32	Unused Field	-	All zeroes.
	uint16_t set_size_le;		//120	4	Volume Set Size	int16_LSB-MSB	The size of the set in this logical volume (number of disks).
	uint16_t set_size_be;
	uint16_t seq_num_le;		//124	4	Volume Sequence Number	int16_LSB-MSB	The number of this disk in the Volume Set.
	uint16_t seq_num_be;
	uint16_t blk_size_le;		//128	4	Logical Block Size	int16_LSB-MSB	The size in bytes of a logical block. NB: This means that a logical block on a CD could be something other than 2 KiB!
	uint16_t blk_size_be;
	uint32_t path_tbl_size_le;	//132	8	Path Table Size	int32_LSB-MSB	The size in bytes of the path table.
	uint32_t path_tbl_size_be;
	uint32_t le_path_tbl;		//140	4	Location of Type-L Path Table	int32_LSB	LBA location of the path table. The path table pointed to contains only little-endian values.
	uint32_t le_path_tbl_opt;	//144	4	Location of the Optional Type-L Path Table	int32_LSB	LBA location of the optional path table. The path table pointed to contains only little-endian values. Zero means that no optional path table exists.
	uint32_t be_path_tbl;		//148	4	Location of Type-M Path Table	int32_MSB	LBA location of the path table. The path table pointed to contains only big-endian values.
	uint32_t be_path_tbl_opt;	//152	4	Location of Optional Type-M Path Table	int32_MSB	LBA location of the optional path table. The path table pointed to contains only big-endian values. Zero means that no optional path table exists.
	iso9660_dir_entry root_dir;	//156	34	Directory entry for the root directory	-	Note that this is not an LBA address, it is the actual Directory Record, which contains a single byte Directory Identifier (0x00), hence the fixed 34 byte size.
	char vol_set_id[128];		//190	128	Volume Set Identifier	strD	Identifier of the volume set of which this volume is a member.
	char pub_id[128];			// 318	128	Publisher Identifier	strA	The volume publisher. For extended publisher information, the first byte should be 0x5F, followed by the filename of a file in the root directory. If not specified, all bytes should be 0x20.
	char data_prep_id[128];		//446	128	Data Preparer Identifier	strA	The identifier of the person(s) who prepared the data for this volume. For extended preparation information, the first byte should be 0x5F, followed by the filename of a file in the root directory. If not specified, all bytes should be 0x20.
	char app_id[128];			//574	128	Application Identifier	strA	Identifies how the data are recorded on this volume. For extended information, the first byte should be 0x5F, followed by the filename of a file in the root directory. If not specified, all bytes should be 0x20.
	char copy_id[38];			//702	38	Copyright File Identifier	strD	Filename of a file in the root directory that contains copyright information for this volume set. If not specified, all bytes should be 0x20.
	char abs_file_is[36];		//740	36	Abstract File Identifier	strD	Filename of a file in the root directory that contains abstract information for this volume set. If not specified, all bytes should be 0x20.
	char bibl_file_id[37];		//776	37	Bibliographic File Identifier	strD	Filename of a file in the root directory that contains bibliographic information for this volume set. If not specified, all bytes should be 0x20.
	uint8_t t_creat[17];		//813	17	Volume Creation Date and Time	dec-datetime	The date and time of when the volume was created.
	uint8_t t_mod[17];			//830	17	Volume Modification Date and Time	dec-datetime	The date and time of when the volume was modified.
	uint8_t t_exp[17];			//847	17	Volume Expiration Date and Time	dec-datetime	The date and time after which this volume is considered to be obsolete. If not specified, then the volume is never considered to be obsolete.
	uint8_t t_eff[17];			//864	17	Volume Effective Date and Time	dec-datetime	The date and time after which the volume may be used. If not specified, the volume may be used immediately.
	uint8_t file_struct_ver;	//881	1	File Structure Version	int8	The directory records and path table version (always 0x01).
	uint8_t unused1;			//882	1	Unused	-	Always 0x00.
	uint8_t undef[512];			//883	512	Application Used	-	Contents not defined by ISO 9660.
	uint8_t reserved[653];		//1395	653	Reserved	-	Reserved by ISO.
};

struct __attribute__((packed)) iso9660_path_table_entry
{
	uint8_t len;			//0	1	Length of Directory Identifier
	uint8_t ext_attr_len;	//1	1	Extended Attribute Record Length
	uint32_t extent;		//2	4	Location of Extent (LBA). This is in a different format depending on whether this is the L-Table or M-Table (see explanation above).
	uint16_t parent;		//6	2	Directory number of parent directory (an index in to the path table). This is the field that limits the table to 65536 records.
	char name[0];			//8	(variable)	Directory Identifier (name) in d-characters.
};

struct iso9660_dir_entry_cache
{
	iso9660_path_table_entry* pte;
	std::vector<std::shared_ptr<iso9660_dir_entry_cache>> nodes;
};

struct iso9660;

struct iso9660_io_handle : public io_handle
{
	iso9660_io_handle(std::shared_ptr<disk_block_io> device, uint32_t start, uint32_t size)
		: device(device), start(start), pos(0), size(size)
	{ }

	size_t read(void* buf, size_t len)
	{
		if (pos + len > size)
			len = size - pos;

		device->read(buf, start + pos, len);
		pos += len;

		return len;
	}

	size_t write(void*, size_t)
	{
		return -1;
	}

	bool close()
	{
		return true;
	}

private:
	std::shared_ptr<disk_block_io> device;

	uint32_t start;
	uint32_t pos;
	uint32_t size;
};

struct iso9660
{
	iso9660(std::shared_ptr<disk_block_io> device)
		: device(device)
	{
		iso9660_dir_entry_cache root;

		std::vector<iso9660_path_table_entry*> pte_all;

		for (int i = 0; ; i++)
		{
			std::shared_ptr<iso9660_vol_desc> vol_desk = read_vold_desk(i);

			if (vol_desk->type == 0x01) /* Primary volume descriptor */
			{
				prim = std::make_shared<iso9660_vol_desc_primary>();
				/* Copy */
				*prim = *reinterpret_cast<iso9660_vol_desc_primary*>(vol_desk.get());
			}
			else if (vol_desk->type == 255)
				break;
		}

		if (!prim)
			panic("ISO9660: No primary volume descriptor found");
	}

	std::shared_ptr<io_handle> open(const char* name)
	{
		con << "neem: " << name << '\n';
		auto dp = find_de_path(&prim->root_dir, name);
		if (!dp)
			return nullptr;

		if (dp->file_flags & 0x02)
			return nullptr;

		/* XXX: GCC bug: https://gcc.gnu.org/bugzilla/show_bug.cgi?id=36566 */
		return std::make_shared<iso9660_io_handle>(device, dp->extent_le * prim->blk_size_le, (uint32_t) dp->data_len_le);
	}

	static bool probe(std::shared_ptr<disk_block_io> device)
	{
		auto vol_desk = read_vold_desk(device, 0);

		return vol_desk->version == 0x01 &&
			vol_desk->ident[0] == 'C' &&
			vol_desk->ident[1] == 'D' &&
			vol_desk->ident[2] == '0' &&
			vol_desk->ident[3] == '0' &&
			vol_desk->ident[4] == '1';
	}

private:
	std::shared_ptr<iso9660_dir_entry> find_de_path(iso9660_dir_entry* de, const char* name)
	{
		std::unique_ptr<char, decltype(&mallocator::free)> sdup(strdup(name), &mallocator::free);
		std::shared_ptr<iso9660_dir_entry> dp;

		char* s = sdup.get();
		const char* se = s;

		while (*s)
		{
			if (*s == '/')
			{
				*s = 0;

				/* Find the current path entry within the current directory entry */
				dp = find_de(de, se);
				if (!dp)
					return nullptr;

				de = dp.get();

				/* New path entry starts at the next character */
				se = s + 1;
			}

			s++;
		}

		return find_de(de, se);
	}

	std::shared_ptr<iso9660_dir_entry> find_de(iso9660_dir_entry* de, const char* name)
	{
		auto extent_dat = std::make_unique<uint8_t[]>(de->data_len_le);
		device->read((void*) extent_dat.get(), de->extent_le * prim->blk_size_le, de->data_len_le);

		auto name_len = strlen(name);

		int idx = 0;
		for (uint32_t i = 0; i < de->data_len_le;)
		{
			iso9660_dir_entry* child = (iso9660_dir_entry*) (((uintptr_t) extent_dat.get()) + i);

			if (!child->len)
				break;

			if (test_name(child, idx, name, name_len))
			{
				auto r = std::make_shared<iso9660_dir_entry>();
				*r = *child;

				return r;
			}

			i += child->len;
			idx++;
		}

		return nullptr;
	}

	bool test_name(iso9660_dir_entry* de, int idx, const char* name, size_t name_len)
	{
		if (idx == 0 && strncmp(name, ".", name_len) == 0)
			return true;

		if (idx == 1 && strncmp(name, "..", name_len) == 0)
			return true;

		return name_len == de->file_id_len && memcmp(name, de->file_id, de->file_id_len) == 0;
	}

	static std::shared_ptr<iso9660_vol_desc> read_vold_desk(std::shared_ptr<disk_block_io> device, int n)
	{
		auto vol_desk = std::make_shared<iso9660_vol_desc>();
		device->read((void*) vol_desk.get(), (0x10 + n) * 2048, sizeof(iso9660_vol_desc));

		return vol_desk;
	}
	std::shared_ptr<iso9660_vol_desc> read_vold_desk(int n) { return read_vold_desk(device, n); }

	std::shared_ptr<disk_block_io> device;
	std::shared_ptr<iso9660_vol_desc_primary> prim;
	//iso9660_vol_desc_primary* prim = nullptr;
};

//"boot/../bla/othefile.txt;1"

std::shared_ptr<iso9660> cd;

void iso9660_test()
{
	auto is_cd = iso9660::probe(hda);

	con << "drive is " << (is_cd ? "" : "not ") << "an iso9660 image\n";

	if (is_cd)
	{
		cd = std::make_shared<iso9660>(hda);
//		iso9660 cdrom(hda);
	}
}

void k_test_init()
{
	test();

	/* Use the top of our current stack. It's safe to smash it, since we won't return.
	 * We'll use 0 for the tss_rsp, because the idle task is never in user space and
	 * will never cause a privilege change. */
	task_idle = cache_alloc<task>::take_shared(0, KERNEL_STACK_TOP, 0);

	task_add(task_idle);

	/* Here we set up our stack for the task. One initial page. The rip entry is set
	 * to the entry point of the function that will, in turn, call user space */
	auto ustack1 = std::make_unique<task_stack>(PAGE_SIZE, (uint64_t) &uspace_jump_trampoline, (uint64_t) &dead_task);
	auto ustack2 = std::make_unique<task_stack>(PAGE_SIZE, (uint64_t) &uspace_jump_trampoline, (uint64_t) &dead_task);

#ifdef TASK_SWITCH_SAVE_CALLER_SAVED
	ustack1->init_regs.rdi = (uint64_t) &k_test_user1;
	ustack1->init_regs.rsi = ustack1->top<uint64_t>();

	ustack2->init_regs.rdi = (uint64_t) &k_test_user2;
	ustack2->init_regs.rsi = ustack2->top<uint64_t>();
#else
	ustack1->init_regs->r12 = (uint64_t) &k_test_user1;
	ustack1->init_regs->r13 = ustack1->top<uint64_t>();
	ustack1->init_regs->r14 = 42;	// Will be passed as arguments
	ustack1->init_regs->r15 = 43;

	ustack2->init_regs->r12 = (uint64_t) &k_test_user2;
	ustack2->init_regs->r13 = ustack2->top<uint64_t>();
	ustack2->init_regs->r14 = 44;
	ustack2->init_regs->r15 = 45;
#endif

	/* Set up the task. Use the registers state (rip and return pointer, as set above) as
	 * stack pointer. This way, when the scheduler comes along and performs the switch to
	 * this task, it will neatly pop those registers, and 'return' to the function
	 * pointed to by state->rip. After which that function performs the actual jump
	 * to userspace. */
	auto task1 = cache_alloc<task>::take_shared(1, std::move(ustack1), std::make_unique<uint8_t[]>(KSTACK_SIZE), KSTACK_SIZE);
	auto task2 = cache_alloc<task>::take_shared(2, std::move(ustack2), std::make_unique<uint8_t[]>(KSTACK_SIZE), KSTACK_SIZE);

	task_add(task1);
	task_add(task2);

	task1->running = true;
	task2->running = true;

	tss0.rsp0 = 0;
	current = task_idle;

	/* Init ata controller */
	ata0 = std::make_shared<ata_pio>(0x1f0, 0x3f6, pic_sys.to_intr(14));
	hda = std::make_shared<ata_disk>(ata0, false);

	/* Enable global interrupts */
	pic_sys.sti();

	/* Start the idle task. Since we will never return from this, we can safely set the
	 * stack pointer to the top of our current stack frame. */
	init_tsk0((uint64_t) tsk_idle, (uint64_t) &dead_task, KERNEL_STACK_TOP);

	panic("Idle task returned!?");
}

int add_handle(std::shared_ptr<io_handle> handle)
{
	int fd = -1;
	/* XXX: LOCK SHIT */
	for (size_t i = 0; i < current->io_handles.size(); i++)
	{   
		if (!current->io_handles[i])
		{   
			fd = i;
			break;
		}
	}

	if (fd == -1)
	{   
		fd = current->io_handles.size();
		/* Push a new one */
		current->io_handles.push_back(handle);
	}
	else
		current->io_handles[fd] = handle;
	/* XXX: Safe. Unlock */

	return (size_t) fd;;
}

int kopen(int arg0, int arg1)
{
	auto handle = devs.open(arg0, arg1);
	if (!handle)
		return -1;

	return add_handle(handle);
}

bool kclose(int fd)
{
	if ((size_t) fd >= current->io_handles.size() || !current->io_handles[fd])
		return -1;

	if (current->io_handles[fd]->close())
	{
		current->io_handles[fd] = nullptr;
		return true;
	}

	return false;
}

size_t kread(int fd, void *buf, size_t len)
{
	if ((size_t) fd >= current->io_handles.size() || !current->io_handles[fd])
		return -1;

	auto handle = current->io_handles[fd];

	return handle->read(buf, len);
}

size_t kwrite(int fd, void *buf, size_t len)
{
	if ((size_t) fd >= current->io_handles.size() || !current->io_handles[fd])
		return -1;

	auto handle = current->io_handles[fd];

	return handle->write(buf, len);
}

extern "C" size_t syscall_handler(uint64_t syscall_no, uint64_t arg0, uint64_t arg1, uint64_t arg2, uint64_t arg3, uint64_t arg4)
{
	auto now = jiffies;
	(void) arg0, (void) arg1, (void) arg2, (void) arg3, (void) arg4;

	switch(syscall_no)
	{
		case 0:
			{
				const unsigned char* s = (const unsigned char*) arg0;
				auto len = arg1;
				for (size_t i = 0; i < len; i++)
					con.putc(*s++);
				break;
			}
		case 1:
			con.putc(arg0);
			break;
		case 5:
			schedule();
			break;

		case 6:
			current->running = false;
			schedule();
			break;

		case 7:
			current->wait_for = [now]() { return (jiffies - now) >= 18; };
			schedule();
			current->wait_for = nullptr;
			break;

		case 9: // Test shit
			ata0->read((void*) arg0, arg1, arg2);
			return 0;

		case 10: // Test shit
			//ata0->read((void*) arg0, arg1, arg2);
			hda->read((void*) arg0, arg1, arg2);
			return 0;
			//ata_test();
			break;
		
		case 11: // Test shit
			hda->write((void*) arg0, arg1, arg2);
			return 0;

		case 12:
			iso9660_test();
			return 0;

		case 0x0d:
			return add_handle(cd->open((const char*) arg0));

		case 0x10:
			return (size_t) kopen(arg0, arg1);
		case 0x11:
			return (size_t) kclose(arg0);
		case 0x13:
			return (size_t) kread(arg0, (void*) arg1, arg2);
		case 0x14:
			return (size_t) kwrite(arg0, (void*) arg1, arg2);
		default:
			panic("Invalid system call");
	}

	return -1;
}

struct timer : public intr_driver
{
	timer()
		: intr_driver(pic_sys.to_intr(0))
	{
		run_scheduler = true;
	}

	void interrupt(uint64_t, interrupt_state*) override
	{
		jiffies++;
	}
};

timer* tmr;

void kmain()
{
	mallocator::test();

	//pic_sys.disable(0x20);
//	for (;;)
//		qemu_out_char('1');
	tmr = new timer();

	kbd_init();

	k_test_init();
}
