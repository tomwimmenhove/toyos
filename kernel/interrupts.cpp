#include "interrupts.h"
#include "debug.h"
#include "memory.h"
#include "malloc.h"
#include "sysregs.h"

void exception_div_by_zero(interrupt_state*){ panic("exception_div_by_zero"); }
void exception_debug(interrupt_state*){ panic("exception_debug"); }
void exception_nmi(interrupt_state*){ panic("exception_nmi"); }
void exception_breakpoint(interrupt_state*){ panic("exception_breakpoint"); }
void exception_ovf(interrupt_state*){ panic("exception_ovf"); }
void exception_bound(interrupt_state*){ panic("exception_bound"); }
void exception_ill(interrupt_state*){ panic("exception_ill"); }
void exception_dev_not_avail(interrupt_state*){ panic("exception_dev_not_avail"); }
void exception_double(interrupt_state*){ panic("exception_double"); }
void exception_invalid_tss(interrupt_state*){ panic("exception_invalid_tss"); }
void exception_seg_not_present(interrupt_state*){ panic("exception_seg_not_present"); }
void exception_stack_seg(interrupt_state*){ panic("exception_stack_seg"); }
void exception_gp(interrupt_state*){ panic("exception_gp"); }
void exception_fp(interrupt_state*){ panic("exception_fp"); }
void exception_align(interrupt_state*){ panic("exception_align"); }
void exception_mach_chk(interrupt_state*){ panic("exception_mach_chk"); }
void exception_simd_fp(interrupt_state*){ panic("exception_simd_fp"); }
void exception_virt(interrupt_state*){ panic("exception_virt"); }
void exception_sec(interrupt_state*){ panic("exception_sec"); }

void exception_page(interrupt_state* state, uint64_t addr)
{
	dbg << "exception_page: err_code=" << state->err_code << " addr=" << addr << '\n';
	memory::handle_pg_fault(state, addr);
	mallocator::handle_pg_fault(state, addr);
}

extern "C" void interrupt_handler(uint64_t irq_num, interrupt_state* state)
{
	dbg << "Interrupt " << irq_num << " err_code: " << state->err_code << " at rip=" << state->rip << '\n';
	switch (irq_num)
	{
		case 0x00: exception_div_by_zero(state); return;
		case 0x01: exception_debug(state); return;
		case 0x02: exception_nmi(state); return;
		case 0x03: exception_breakpoint(state); return;
		case 0x04: exception_ovf(state); return;
		case 0x05: exception_bound(state); return;
		case 0x06: exception_ill(state); return;
		case 0x07: exception_dev_not_avail(state); return;
		case 0x08: exception_double(state); return;
		case 0x0a: exception_invalid_tss(state); return;
		case 0x0b: exception_seg_not_present(state); return;
		case 0x0c: exception_stack_seg(state); return;
		case 0x0d: exception_gp(state); return;
		case 0x0e: exception_page(state, cr2_get()); return;
		case 0x10: exception_fp(state); return;
		case 0x11: exception_align(state); return;
		case 0x12: exception_mach_chk(state); return;
		case 0x13: exception_simd_fp(state); return;
		case 0x14: exception_virt(state); return;
		case 0x1e: exception_sec(state); return;
		default: break;
	}
}


