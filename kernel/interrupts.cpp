#include "interrupts.h"
#include "debug.h"
#include "memory.h"
#include "malloc.h"
#include "sysregs.h"
#include "pic.h"

extern "C" void exception_unhandled(interrupt_state*){ panic("exception_unhandled"); }
extern "C" void exception_div_by_zero(interrupt_state*){ panic("exception_div_by_zero"); }
extern "C" void exception_debug(interrupt_state*){ panic("exception_debug"); }
extern "C" void exception_nmi(interrupt_state*){ panic("exception_nmi"); }
extern "C" void exception_breakpoint(interrupt_state*){ panic("exception_breakpoint"); }
extern "C" void exception_ovf(interrupt_state*){ panic("exception_ovf"); }
extern "C" void exception_bound(interrupt_state*){ panic("exception_bound"); }
extern "C" void exception_ill(interrupt_state*){ panic("exception_ill"); }
extern "C" void exception_dev_not_avail(interrupt_state*){ panic("exception_dev_not_avail"); }
extern "C" void exception_double(interrupt_state*){ panic("exception_double"); }
extern "C" void exception_invalid_tss(interrupt_state*){ panic("exception_invalid_tss"); }
extern "C" void exception_seg_not_present(interrupt_state*){ panic("exception_seg_not_present"); }
extern "C" void exception_stack_seg(interrupt_state*){ panic("exception_stack_seg"); }
extern "C" void exception_gp(interrupt_state* state)
{
	con << "GP fault at rip=0x" << hex_u64(state->iregs.rip)
		<< " with err_code=0c" << hex_u16(state->err_code) << '\n';
	panic("exception_gp");
}
extern "C" void exception_fp(interrupt_state*){ panic("exception_fp"); }
extern "C" void exception_align(interrupt_state*){ panic("exception_align"); }
extern "C" void exception_mach_chk(interrupt_state*){ panic("exception_mach_chk"); }
extern "C" void exception_simd_fp(interrupt_state*){ panic("exception_simd_fp"); }
extern "C" void exception_virt(interrupt_state*){ panic("exception_virt"); }
extern "C" void exception_sec(interrupt_state*){ panic("exception_sec"); }
extern "C" void exception_page(interrupt_state* state)
{
	uint64_t addr = cr2_get();
//	con << "exception_page: rip=0x" << hex_u64(state->iregs.rip) << " addr=0x" << hex_u64(addr) << '\n';

	if (addr < 0x8000000000000000)
		panic("addr < 0x8000000000000000\n");


	memory::handle_pg_fault(state, addr);
	mallocator::handle_pg_fault(state, addr);
}

interrupts::irq_handler interrupts::irq_handlers[256];

extern "C" void interrupt_pic(int irq_num);
extern "C" void interrupt_handler(uint64_t irq_num, interrupt_state* state)
{
//	con << "Interrupt " << irq_num << " at rip=" << state->iregs.rip << '\n';

	pic_sys.eoi(irq_num);
	interrupts::handle(irq_num, state);
}


