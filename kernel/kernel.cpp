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
#include "iso9660.h"
#include "elf.h"

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
std::shared_ptr<task> current;;

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

uint8_t ata_buf1[128 * 1024];
extern "C" void k_test_user1(uint64_t arg0, uint64_t arg1)
{
	int fd = open(0, 0);
	console_user ucon(fd);

    ucon << "tsk 1: arg0: " << arg0 << '\n';
	ucon << "tsk 1: arg1: " << arg1 << '\n';

	/* Hacky-as-fuck cd init thing */
	syscall((int) syscall_idx::mount_cd );

	/* Open a file on the cd */
	auto elf_fd = open("test/userbin");
	elf64 e(elf_fd);
	if (!e.load())
	{
		ucon << "Loading elf failed\n";
		for (;;)
			;
	}

	/* Jump to the motherfucker */
	typedef int (*elf_entry_fn)();
	elf_entry_fn elf_entry = (elf_entry_fn) e.ehdr.e_entry;
	int returned = elf_entry();

	ucon << "It returned: " << returned << '\n';

	uint8_t abuf[512];
	
	syscall((int) syscall_idx::debug_test_read2, (uint64_t) abuf, 0x180, 6);
	ucon << "partial: \"" << (const char*) abuf << "\"\n";

	/* Test partial write */
	abuf[0] = 42;
	syscall((int) syscall_idx::debug_test_write, (uint64_t) abuf, 2, 1);
	syscall((int) syscall_idx::debug_test_read2, (uint64_t) ata_buf1, 0, 512);

	volatile uint64_t* volatile stack_tst = (uint64_t*) (0x800000000000 - 16);
	*stack_tst = 0;
	uint64_t countah = 0;

	for (;;)
	{
		syscall((int) syscall_idx::debug_test_read2, (uint64_t) abuf, 0, 512);

		if (memcmp(ata_buf1, abuf, 512) != 0)
		{
			ucon << "1: ATA FUCKED UP\n";
			asm volatile("hlt"); // Cause crash
		}
		
//		ucon << "inc==" << hex_u64(*stack_tst) << '\n';
		(*stack_tst)++;
		countah++;

		auto t = *stack_tst;

		if (countah != t)
			ucon << "1: Per-process stack is fucked: " << hex_u64(t) << '\n';
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

	volatile uint64_t* volatile stack_tst = (uint64_t*) (0x800000000000 - 16);
	*stack_tst = 42;

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

		//ucon << "42==" << hex_u64(*stack_tst) << '\n';

		volatile uint64_t  t = *stack_tst;
		
		if (42 != t)
			ucon << "2: Per-process stack is fucked: " << hex_u64(t) << '\n';

		for (ssize_t i = 0; i < len; i++)
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
	cr3_set(current->cr3);
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

int kopen(const char* arg0)
{
	auto handle = cd->open(arg0);
	if (!handle)
		return -1;
	return add_handle(handle);
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

	current->io_handles[fd] = nullptr;
	
	return true;
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

size_t kfsize(int fd)
{
	if ((size_t) fd >= current->io_handles.size() || !current->io_handles[fd])
		return -1;

	return current->io_handles[fd]->size();
}

size_t kseek(int fd, size_t pos)
{
	if ((size_t) fd >= current->io_handles.size() || !current->io_handles[fd])
		return -1;

	return current->io_handles[fd]->seek(pos);
}

size_t kfmap(int fd, uint64_t file_offs, uint64_t addr, uint64_t len)
{
	if ((size_t) fd >= current->io_handles.size() || !current->io_handles[fd])
		return -1;

	mapped_io_handle mih { current->io_handles[fd], fd, file_offs, addr, len };
	current->mapped_io_handles.push_front(mih);

	return 0;
}

size_t kfunmap(int fd, uint64_t addr)
{
	// XXX: FIXME. Always returns success. c++20 remove_if returns number of elements removed.

	current->mapped_io_handles.remove_if([fd, addr](mapped_io_handle mih) { return mih.fd == fd && mih.addr == addr; });

	return 0;
}

extern "C" size_t syscall_handler(uint64_t syscall_no, uint64_t arg0, uint64_t arg1, uint64_t arg2, uint64_t arg3, uint64_t arg4)
{
	auto now = jiffies;
	(void) arg0, (void) arg1, (void) arg2, (void) arg3, (void) arg4;

	switch((syscall_idx) syscall_no)
	{
		case syscall_idx::debug_outs:
			{
				const unsigned char* s = (const unsigned char*) arg0;
				auto len = arg1;
				for (size_t i = 0; i < len; i++)
					con.putc(*s++);
				break;
			}
		case syscall_idx::debug_outc:
			con.putc(arg0);
			break;
		case syscall_idx::yield:
			schedule();
			break;

		case syscall_idx::stop:
			current->running = false;
			schedule();
			break;

		case syscall_idx::debug_test_timer:
			current->wait_for = [now]() { return (jiffies - now) >= 18; };
			schedule();
			current->wait_for = nullptr;
			break;

		case syscall_idx::debug_test_read1: // Test shit
			ata0->read((void*) arg0, arg1, arg2);
			return 0;

		case syscall_idx::debug_test_read2: // Test shit
			//ata0->read((void*) arg0, arg1, arg2);
			hda->read((void*) arg0, arg1, arg2);
			return 0;
		
		case syscall_idx::debug_test_write: // Test shit
			hda->write((void*) arg0, arg1, arg2);
			return 0;

		case syscall_idx::debug_clone_tlb:
			{
				auto cloned_table = memory::clone_tables();
				cr3_set(cloned_table);
			}
			break;

		case syscall_idx::mount_cd:
			iso9660_test();
			return 0;

			case syscall_idx::open_file:
			return kopen((const char*) arg0);
		case syscall_idx::open_dev:
			return kopen(arg0, arg1);
		case syscall_idx::close:
			return kclose(arg0);
		case syscall_idx::read:
			return kread(arg0, (void*) arg1, arg2);
		case syscall_idx::write:
			return kwrite(arg0, (void*) arg1, arg2);
		case syscall_idx::seek:
			return kseek(arg0, arg1);
		case syscall_idx::fsize:
			return kfsize(arg0);

		case syscall_idx::fmap:
			return kfmap(arg0, arg1, arg2, arg3);
		case syscall_idx::funmap:
			return kfunmap(arg0, arg1);
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
