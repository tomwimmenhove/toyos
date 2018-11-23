#include <stdint.h>
#include <stddef.h>
#include <memory>
#include <embxx/container/StaticQueue.h>

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

void print_stack_use()
{
	con << "stack usuage: " << (KERNEL_STACK_TOP - (uint64_t) __builtin_frame_address(0)) << "\n";
}

void dead_task()
{
	panic("Can't handle returning tasks yet");
}

uint64_t jiffies = 0;

void schedule();

std::shared_ptr<task> tasks;
std::shared_ptr<task> current;

std::shared_ptr<task> task_idle;

devices devs;

void test()
{
	std::vector<int> tst;
	for(int i = 0; i < 10000000; i++)
		tst.push_back(i);

	for(int i = 0; i < 10000000; i++)
		assert(tst[i] == i);


	auto drv_kbd = cache_alloc<driver_tty>::take_shared();

	drv_kbd->dev_type = 0;

	devs.add(drv_kbd);
}

extern "C" void k_test_user1(uint64_t arg0, uint64_t arg1)
{
	int fd = open(0, 0);
	console_user ucon(fd);

    ucon << "tsk 1: arg0: " << arg0 << '\n';
	ucon << "tsk 1: arg1: " << arg1 << '\n';

	for (;;)
	{
		ucon << "1";
		syscall(7);
	}
}

void k_test_user2(uint64_t arg0, uint64_t arg1)
{
	int fd = open(0, 0);
	console_user ucon(fd);

    ucon << "tsk 2: arg0: " << arg0 << '\n';
	ucon << "tsk 2: arg1: " << arg1 << '\n';

	char buf[100];
	for (;;)
	{
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

//	con << "Switched to task " << current->id << '\n';

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

class ata_pio : public intr_driver
{
public:
	ata_pio(uint16_t io_addr, uint16_t io_alt_addr)
		: io_addr(io_addr), io_alt_addr(io_alt_addr)
	{

		for (int i = 0x20; i < 0x30; i++)
		{
			pic_sys.enable(i);
			pic_sys.eoi(i);
		}

		con << "ATA test\n";

		dev_select(false, true);

		/* Reset */
		outb(6, io_alt_addr + io_alt_ctrl);
		outb(2, io_alt_addr + io_alt_ctrl);
		wait_busy(); /* Do we need to wait? */
	}

	void enable_irq()
	{
		outb(0, io_alt_addr + io_alt_ctrl);
	}

	void dev_select(bool slave, bool force = false)
	{
		if (!force && slave == slave_active)
			return;

		outb((uint8_t) drive_reg::always | (uint8_t) drive_reg::lba | (slave ? (uint8_t) drive_reg::slave : 0), io_addr + io_drive);

		wait_busy();
	}

	void wait_busy()
	{
		inb(io_alt_addr + io_alt_status);
		inb(io_alt_addr + io_alt_status);
		inb(io_alt_addr + io_alt_status);
		inb(io_alt_addr + io_alt_status);

		for (;;)
			if ((inb(io_alt_addr + io_alt_status) & (uint8_t) status_reg::bsy) == 0)
				break;
	}

	void read_48(void* buffer, uint64_t lba, int sect_cnt, embxx::util::StaticFunction<void()> callback)
	{
		assert(sect_cnt > 0 && sect_cnt <= 65536);
		if (sect_cnt == 65536)
			sect_cnt = 0;

		read_cmd = true;
		n_sects = sect_cnt;
		buf = (uint16_t*) buffer;
		cb = callback;

		outb(sect_cnt >> 8,			io_addr + io_sect_cnt);
		outb((lba >> 24) & 0xff,	io_addr + io_sect_num);
		outb((lba >> 32) & 0xff,	io_addr + io_cyl_low);
		outb((lba >> 40) & 0xff,	io_addr + io_cyl_high);

		outb(sect_cnt & 0xff,		io_addr + io_sect_cnt);
		outb(lba & 0xff,			io_addr + io_sect_num);
		outb((lba >> 8) & 0xff,		io_addr + io_cyl_low);
		outb((lba >> 16) & 0xff, 	io_addr + io_cyl_high);

		outb(0x24, io_addr + io_cmd);
	}

	void interrupt(uint64_t, interrupt_state*) override
	{
		if (read_cmd)
		{
			for (int i = 0; i < 256 * n_sects; i++)
				buf[i] = inw(io_addr + io_data);

			cb();
		}
		else
		{
		}
	}

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

private:
	uint16_t io_addr;
	uint16_t io_alt_addr;
	bool slave_active = false;

	bool read_cmd = false;
	uint16_t n_sects;
	uint16_t* buf;
	embxx::util::StaticFunction<void()> cb;

	static constexpr int16_t io_data			= 0;
	static constexpr int16_t io_error			= 1;
	static constexpr int16_t io_features		= 1;
	static constexpr int16_t io_sect_cnt		= 2;
	static constexpr int16_t io_sect_num		= 3;
	static constexpr int16_t io_cyl_low			= 4;
	static constexpr int16_t io_cyl_high		= 5;
	static constexpr int16_t io_drive			= 6;
	static constexpr int16_t io_status			= 7;
	static constexpr int16_t io_cmd				= 7;

	static constexpr int16_t io_alt_status		= 0;
	static constexpr int16_t io_alt_ctrl		= 0;
	static constexpr int16_t io_alt_drv_addr	= 1;
};

ata_pio ata0(0x1f0, 0x3f6);

uint8_t ata_buf[128 * 1024];
static void ata_test()
{
	interrupts::add(pic_sys.to_intr(14), &ata0);

	ata0.enable_irq();

	embxx::util::StaticFunction<void()> cb
	{
		[]()->void
		{
			for (int i = 0; i < 512; i++)
				con << hex_u8(ata_buf[i]) << "  ";
		}
	};

	ata0.read_48((void*) ata_buf, 0x8000 / 512, 1, cb);
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

	/* Enable global interrupts */
	pic_sys.sti();

	ata_test();

	/* Start the idle task. Since we will never return from this, we can safely set the
	 * stack pointer to the top of our current stack frame. */
	init_tsk0((uint64_t) tsk_idle, (uint64_t) &dead_task, KERNEL_STACK_TOP);

	panic("Idle task returned!?");
}

int kopen(int arg0, int arg1)
{
	auto handle = devs.open(arg0, arg1);
	if (!handle)
		return -1;

	int fd = -1;
	/* XXX: LOCK SHIT */
	for (size_t i = 0; i < current->dev_handles.size(); i++)
	{
		if (!current->dev_handles[i])
		{
			fd = i;
			break;
		}
	}

	if (fd == -1)
	{
		fd = current->dev_handles.size();
		/* Push a new one */
		current->dev_handles.push_back(handle);
	}
	else
		current->dev_handles[fd] = handle;
	/* XXX: Safe. Unlock */

	return (size_t) fd;;
}

bool kclose(int fd)
{
	if ((size_t) fd >= current->dev_handles.size() || !current->dev_handles[fd])
		return -1;

	if (current->dev_handles[fd]->close())
	{
		current->dev_handles[fd] = nullptr;
		return true;
	}

	return false;
}

size_t kread(int fd, void *buf, size_t len)
{
	if ((size_t) fd >= current->dev_handles.size() || !current->dev_handles[fd])
		return -1;

	auto handle = current->dev_handles[fd];

	return handle->read(buf, len);
}

size_t kwrite(int fd, void *buf, size_t len)
{
	if ((size_t) fd >= current->dev_handles.size() || !current->dev_handles[fd])
		return -1;

	auto handle = current->dev_handles[fd];

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
	{
		run_scheduler = true;
	}

	void interrupt(uint64_t, interrupt_state*) override
	{
		jiffies++;
	}
};

timer tmr;

void kmain()
{
	mallocator::test();

	interrupts::add(pic_sys.to_intr(0), &tmr);

	kbd_init();

	k_test_init();
}
