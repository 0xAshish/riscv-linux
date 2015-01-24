#include <linux/mm.h>
#include <linux/kernel.h>
#include <linux/interrupt.h>
#include <linux/perf_event.h>
#include <linux/signal.h>

#include <asm/pgalloc.h>
#include <asm/ptrace.h>
#include <asm/uaccess.h>

asmlinkage void do_page_fault(struct pt_regs *regs)
{
	struct task_struct *tsk;
	struct vm_area_struct *vma;
	struct mm_struct *mm;
	unsigned long addr, epc, cause, fault;
	unsigned int write, flags;
	int sicode;

	cause = regs->cause;
	write = (cause == EXC_STORE_ACCESS);
	flags = FAULT_FLAG_ALLOW_RETRY | FAULT_FLAG_KILLABLE
		| (write ? FAULT_FLAG_WRITE : 0);
	epc = regs->epc;
	addr = (cause == EXC_INST_ACCESS) ? epc : regs->badvaddr;

	sicode = SEGV_MAPERR;

	tsk = current;
	mm = tsk->mm;

	/*
	 * Fault-in kernel-space virtual memory on-demand.
	 * The 'reference' page table is init_mm.pgd.
	 *
	 * NOTE! We MUST NOT take any locks for this case. We may
	 * be in an interrupt or a critical region, and should
	 * only copy the information from the master page table,
	 * nothing more.
	 */
	if (unlikely((addr >= VMALLOC_START) && (addr <= VMALLOC_END)))
		goto vmalloc_fault;

	 /* Enable interrupts if they were previously enabled in the
	    parent context */
	if (likely(regs->status & SR_PEI))
		local_irq_enable();

	/* Do not take the fault if within an interrupt
	   or if lacking a user context */
	if (!mm || in_atomic())
		goto no_context;

	if (user_mode(regs))
		flags |= FAULT_FLAG_USER;

retry:
	down_read(&mm->mmap_sem);
	vma = find_vma(mm, addr);
	if (unlikely(!vma))
		goto bad_area;
	if (likely(vma->vm_start <= addr))
		goto good_area;
	if (unlikely(!(vma->vm_flags & VM_GROWSDOWN)))
		goto bad_area;
	if (unlikely(expand_stack(vma, addr)))
		goto bad_area;

good_area:
	sicode = SEGV_ACCERR;

	if (unlikely(write && (!(vma->vm_flags & VM_WRITE)))) {
		goto bad_area;
	}
	if (unlikely((cause == EXC_INST_ACCESS)
		&& (!(vma->vm_flags & VM_EXEC)))) {
		goto bad_area;
	}

	/*
	 * If for any reason at all we could not handle the fault,
	 * make sure we exit gracefully rather than endlessly redo
	 * the fault.
	 */
	fault = handle_mm_fault(mm, vma, addr, flags);

	if ((fault & VM_FAULT_RETRY) && fatal_signal_pending(tsk))
		return;


	perf_sw_event(PERF_COUNT_SW_PAGE_FAULTS, 1, regs, addr);
	if (unlikely(fault & VM_FAULT_ERROR)) {
		if (fault & VM_FAULT_OOM) {
			goto out_of_memory;
		} else if (fault & VM_FAULT_SIGBUS) {
			goto do_sigbus;
		}
		BUG();
	}

	if (flags & FAULT_FLAG_ALLOW_RETRY) {
		if (fault & VM_FAULT_MAJOR) {
			perf_sw_event(PERF_COUNT_SW_PAGE_FAULTS_MAJ, 1, regs, addr);
			tsk->maj_flt++;
		} else {
			perf_sw_event(PERF_COUNT_SW_PAGE_FAULTS_MIN, 1, regs, addr);
			tsk->min_flt++;
		}
		if (fault & VM_FAULT_RETRY) {
			flags &= ~(FAULT_FLAG_ALLOW_RETRY);
//			flags |= FAULT_FLAG_TRIED;

			/*
			 * No need to up_read(&mm->mmap_sem) as we would
			 * have already released it in __lock_page_or_retry
			 * in mm/filemap.c.
			 */
			goto retry;
		}
	}

	up_read(&mm->mmap_sem);
	return;

bad_area:
	up_read(&mm->mmap_sem);
	/* User mode accesses cause a SIGSEGV */
	if (user_mode(regs)) {
		do_trap(regs, SIGSEGV, sicode, addr, tsk);
		return;
	}

no_context:
	/* Are we prepared to handle this fault as an exception? */
	if (fixup_exception(regs)) {
		return;
	}
	pr_alert("Unable to handle kernel paging request at virtual "
		"address " REG_FMT " epc=" REG_FMT, addr, epc);
	die("Oops", regs, 0);

out_of_memory:
	up_read(&mm->mmap_sem);
	if (!user_mode(regs))
		goto no_context;
	pagefault_out_of_memory();
	return;

do_sigbus:
	up_read(&mm->mmap_sem);
	if (!user_mode(regs))
		goto no_context;
	do_trap(regs, SIGBUS, BUS_ADRERR, addr, tsk);
	return;

vmalloc_fault:
	{
		pgd_t *pgd, *pgd_k;
		pud_t *pud, *pud_k;
		pmd_t *pmd, *pmd_k;
		pte_t *pte_k;
		int index;

	        if (user_mode(regs))
			goto bad_area;

		/*
		 * Synchronize this task's top level page-table
		 * with the 'reference' page table.
		 *
		 * Do _not_ use "tsk->active_mm->pgd" here.
		 * We might be inside an interrupt in the middle
		 * of a task switch.
		 */
		index = pgd_index(addr);
		pgd = (pgd_t *)__va(csr_read(ptbr)) + index;
		pgd_k = init_mm.pgd + index;

		if (!pgd_present(*pgd_k))
			goto no_context;
		set_pgd(pgd, *pgd_k);

		pud = pud_offset(pgd, addr);
		pud_k = pud_offset(pgd_k, addr);
		if (!pud_present(*pud_k))
			goto no_context;

		/* Since the vmalloc area is global, it is unnecessary
		   to copy individual PTEs */
		pmd = pmd_offset(pud, addr);
		pmd_k = pmd_offset(pud_k, addr);
		if (!pmd_present(*pmd_k))
			goto no_context;
		set_pmd(pmd, *pmd_k);

		/* Make sure the actual PTE exists as well to
		 * catch kernel vmalloc-area accesses to non-mapped
		 * addresses. If we don't do this, this will just
		 * silently loop forever.
		 */
		pte_k = pte_offset_kernel(pmd_k, addr);
		if (!pte_present(*pte_k))
			goto no_context;
		return;
	}
}
