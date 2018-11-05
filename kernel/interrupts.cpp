#include "interrupts.h"
#include "debug.h"
#include "memory.h"
#include "malloc.h"
#include "sysregs.h"
#include "pic.h"

extern "C" void exception_unhandled(exception_state*){ panic("exception_unhandled"); }
extern "C" void exception_div_by_zero(exception_state*){ panic("exception_div_by_zero"); }
extern "C" void exception_debug(exception_state*){ panic("exception_debug"); }
extern "C" void exception_nmi(exception_state*){ panic("exception_nmi"); }
extern "C" void exception_breakpoint(exception_state*){ panic("exception_breakpoint"); }
extern "C" void exception_ovf(exception_state*){ panic("exception_ovf"); }
extern "C" void exception_bound(exception_state*){ panic("exception_bound"); }
extern "C" void exception_ill(exception_state*){ panic("exception_ill"); }
extern "C" void exception_dev_not_avail(exception_state*){ panic("exception_dev_not_avail"); }
extern "C" void exception_double(exception_state*){ panic("exception_double"); }
extern "C" void exception_invalid_tss(exception_state*){ panic("exception_invalid_tss"); }
extern "C" void exception_seg_not_present(exception_state*){ panic("exception_seg_not_present"); }
extern "C" void exception_stack_seg(exception_state*){ panic("exception_stack_seg"); }
extern "C" void exception_gp(exception_state*){ panic("exception_gp"); }
extern "C" void exception_fp(exception_state*){ panic("exception_fp"); }
extern "C" void exception_align(exception_state*){ panic("exception_align"); }
extern "C" void exception_mach_chk(exception_state*){ panic("exception_mach_chk"); }
extern "C" void exception_simd_fp(exception_state*){ panic("exception_simd_fp"); }
extern "C" void exception_virt(exception_state*){ panic("exception_virt"); }
extern "C" void exception_sec(exception_state*){ panic("exception_sec"); }
extern "C" void exception_page(exception_state* state)
{
	uint64_t addr = cr2_get();
	dbg << "exception_page: rip=" << state->iregs.rip << " addr=" << addr << '\n';
	memory::handle_pg_fault(state, addr);
	mallocator::handle_pg_fault(state, addr);
}

interrupts::irq_handler interrupts::irq_handlers[256];

extern "C" void interrupt_pic(int irq_num);
extern "C" void interrupt_handler(uint64_t irq_num, interrupt_state* state)
{
//	dbg << "Interrupt " << irq_num << " at rip=" << state->iregs.rip << '\n';

	interrupts::handle(irq_num, state);

	pic_sys.eoi(irq_num);
}


