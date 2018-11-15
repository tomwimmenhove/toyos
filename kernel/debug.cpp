#include "debug.h"
#include "console.h"

void die()
{
	asm volatile("cli");
#if 0
	asm volatile("movl $0, %eax");
	asm volatile("out %eax, $0xf4");
	asm volatile("cli");
#endif
	for (;;)
	{
		asm volatile("hlt");
	}
}

void panic(const char* msg, interrupt_state* regs)
{
	if (regs)
	{
		con << "KERNEL PANIC: " << msg << '\n';
		con << "Registers: \n";
		con << "rsp: 0x" << hex_u64(regs->rsp) << ' ';
		con << "rsi: 0x" << hex_u64(regs->rsi) << '\n';
		con << "rdi: 0x" << hex_u64(regs->rdi) << ' ';
		con << "rax: 0x" << hex_u64(regs->rax) << '\n';
		con << "rcx: 0x" << hex_u64(regs->rcx) << ' ';
		con << "rdx: 0x" << hex_u64(regs->rdx) << '\n';
		con << "rbx: 0x" << hex_u64(regs->rbx) << ' ';
		con << "rbp: 0x" << hex_u64(regs->rbp) << '\n';
		con << "r8 : 0x" << hex_u64(regs->r8) << ' ';
		con << "r9 : 0x" << hex_u64(regs->r9) << '\n';
		con << "r10: 0x" << hex_u64(regs->r10) << ' ';
		con << "r11: 0x" << hex_u64(regs->r11) << '\n';
		con << "r12: 0x" << hex_u64(regs->r12) << ' ';
		con << "r13: 0x" << hex_u64(regs->r13) << '\n';
		con << "r14: 0x" << hex_u64(regs->r14) << ' ';
		con << "r15: 0x" << hex_u64(regs->r15) << '\n';

		con << "\nerr_code: 0x" << hex_u64(regs->err_code) << '\n';

		con << "\niret regs:\n";
		con << "rip:    0x" << hex_u64(regs->iregs.rip) << ' ';
		con << "rsp:    0x" << hex_u64(regs->iregs.rsp) << '\n';
		con << "cs :    0x" << hex_u16(regs->iregs.cs) << ' ';
		con << "            ss :    0x" << hex_u16(regs->iregs.ss) << '\n';
		con << "rflags: 0x" << hex_u64(regs->iregs.rflags) << '\n';

		con << "\nCut&Paste:\nobjdump --disassemble kernel/kernel.elf --start-address=0x" << hex_u64(regs->iregs.rip) << "|less\n";
	}
	else
		con << "KERNEL PANIC: " << msg << '\n';

	con << "\nHalted.";
	die();
}

