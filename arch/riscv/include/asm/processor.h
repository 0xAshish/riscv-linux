#ifndef _ASM_RISCV_PROCESSOR_H
#define _ASM_RISCV_PROCESSOR_H

/* Status register flags */
#define SR_ET   0x00000001 /* Enable exceptions */
#define SR_EF   0x00000002 /* Enable floating-point */
#define SR_EV   0x00000004 /* Enable vector unit */
#define SR_PS   0x00000010 /* Previous supervisor */
#define SR_S    0x00000020 /* Supervisor */
#define SR_U64  0x00000040 /* RV64 user mode */
#define SR_S64  0x00000080 /* RV64 supervisor mode */
#define SR_VM   0x00000100 /* Enable virtual memory */
#define SR_IM   0x00FF0000 /* Interrupt mask */

#ifndef __ASSEMBLY__

//#include <linux/cpumask.h>
#include <linux/threads.h>
#include <asm/segment.h>

struct task_struct;
struct pt_regs;

/*
 * System setup and hardware flags..
 */
extern void (*cpu_wait)(void);

extern long kernel_thread(int (*fn)(void *), void *arg, unsigned long flags);
extern unsigned long thread_saved_pc(struct task_struct *tsk);
extern void start_thread(struct pt_regs *regs,
			unsigned long pc, unsigned long sp);
extern unsigned long get_wchan(struct task_struct *p);

/*
 * Return current * instruction pointer ("program counter").
 */
#define current_text_addr() ({ __label__ _l; _l: &&_l; })

#define cpu_relax()		barrier()
#define release_thread(thread)	do {} while (0)
#define prepare_to_copy(tsk)	do {} while (0)

/*
 * User space process size: 2GB. This is hardcoded into a few places,
 * so don't change it unless you know what you are doing.
 */
#define TASK_SIZE	0x7fff8000UL

/*
 * This decides where the kernel will search for a free chunk of vm
 * space during mmap's.
 */
#define TASK_UNMAPPED_BASE	((TASK_SIZE / 3) & ~(PAGE_SIZE))

#ifdef __KERNEL__
#define STACK_TOP	TASK_SIZE
#define STACK_TOP_MAX	TASK_SIZE
#endif

/* CPU-specific state of a task */
struct thread_struct {
	unsigned long sp;
	unsigned long status;
	unsigned long pc;
};

#define INIT_THREAD {		\
	.sp = sizeof(init_stack) + (long)&init_stack, \
}

 #define kstk_tos(tsk)		\
	((unsigned long)task_stack_page(tsk) + THREAD_SIZE - 32)
#define task_pt_regs(tsk)	((struct pt_regs *)kstk_tos(tsk) - 1)

#define KSTK_EIP(tsk)		(task_pt_regs(tsk)->pc)
#define KSTK_ESP(tsk)		(task_pt_regs(tsk)->sp)

#endif /* __ASSEMBLY__ */

#endif /* _ASM_RISCV_PROCESSOR_H */
