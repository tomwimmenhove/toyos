#ifndef _CONFIG_H
#define _CONFIG_H

#ifndef __ASSEMBLY__ 
extern "C"
{
	#include "../common/config.h"
}
#endif

//#define TASK_SWITCH_SAVE_CALLER_SAVED

#define USER_PS_STACK_SIZE	(1024 * 1024 * 1)

#define KERNEL_CS	0x08
#define KERNEL_DS	0x10

#define INTR_CS		0x18

#define TSS0		0x20

#define USER_CS		0x30
#define USER_DS		0x38

#define USER_CS_PL3	(USER_CS + 3)
#define USER_DS_PL3	(USER_DS + 3)

#define PAGE_SIZE	0x1000

#define KSTACK_SIZE	(PAGE_SIZE)

#define SYSCALL_INTR	42

#endif /* _CONFIG_H */

