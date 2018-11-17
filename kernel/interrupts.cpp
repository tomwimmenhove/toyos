#include "interrupts.h"
#include "debug.h"
#include "mem.h"
#include "malloc.h"
#include "sysregs.h"
#include "pic.h"

extern "C" void exception_unhandled(interrupt_state* state){ panic("exception_unhandled", state); }
extern "C" void exception_div_by_zero(interrupt_state* state){ panic("exception_div_by_zero", state); }
extern "C" void exception_debug(interrupt_state* state){ panic("exception_debug", state); }
extern "C" void exception_nmi(interrupt_state* state){ panic("exception_nmi", state); }
extern "C" void exception_breakpoint(interrupt_state* state){ panic("exception_breakpoint", state); }
extern "C" void exception_ovf(interrupt_state* state){ panic("exception_ovf", state); }
extern "C" void exception_bound(interrupt_state* state){ panic("exception_bound", state); }
extern "C" void exception_ill(interrupt_state* state){ panic("exception_ill", state); }
extern "C" void exception_dev_not_avail(interrupt_state* state){ panic("exception_dev_not_avail", state); }
extern "C" void exception_double(interrupt_state* state){ panic("exception_double", state); }
extern "C" void exception_invalid_tss(interrupt_state* state){ panic("exception_invalid_tss", state); }
extern "C" void exception_seg_not_present(interrupt_state* state){ panic("exception_seg_not_present", state); }
extern "C" void exception_stack_seg(interrupt_state* state){ panic("exception_stack_seg", state); }
extern "C" void exception_gp(interrupt_state* state)
{
	con << "GP fault at rip=0x" << hex_u64(state->iregs.rip)
		<< " with err_code=0c" << hex_u16(state->err_code) << '\n';
	panic("exception_gp", state);
}
extern "C" void exception_fp(interrupt_state* state){ panic("exception_fp", state); }
extern "C" void exception_align(interrupt_state* state){ panic("exception_align", state); }
extern "C" void exception_mach_chk(interrupt_state* state){ panic("exception_mach_chk", state); }
extern "C" void exception_simd_fp(interrupt_state* state){ panic("exception_simd_fp", state); }
extern "C" void exception_virt(interrupt_state* state){ panic("exception_virt", state); }
extern "C" void exception_sec(interrupt_state* state){ panic("exception_sec", state); }
extern "C" void exception_page(interrupt_state* state)
{
	uint64_t addr = cr2_get();
//	con << "exception_page: rip=0x" << hex_u64(state->iregs.rip) << " addr=0x" << hex_u64(addr) << '\n';

	if (addr < 0x8000000000000000)
		panic("addr < 0x8000000000000000\n", state);


	memory::handle_pg_fault(state, addr);
	mallocator::handle_pg_fault(state, addr);
}

intr_regist interrupts::registrars[256];
int interrupts::nest = 0;

void schedule();

void interrupts::init()
{
	for (int i = 0; i < 256; i++)
		unregist(i);
}

void interrupts::regist(uint8_t intr, intr_regist::irq_handler handler, bool run_scheduler)
{
	auto& registrar = registrars[intr];
	registrar.handler = handler;
	registrar.run_scheduler = run_scheduler;
}

void interrupts::unregist(uint8_t intr) { registrars[intr].handler = nullptr; }
void interrupts::handle(uint64_t irq_num, interrupt_state* state)
{
	nest++;

	auto registrar = registrars[irq_num];
	if (registrar.handler)
	{
		registrar.handler(irq_num, state);
		if (registrar.run_scheduler && nest == 1)
			schedule();
	}
	else
		con << "Interrupt " << irq_num << " has no handler\n";

	nest--;
}

extern "C" void interrupt_handler(uint64_t irq_num, interrupt_state* state)
{
	interrupts::handle(irq_num, state);
	pic_sys.eoi(irq_num);
}


