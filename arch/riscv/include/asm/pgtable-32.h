#ifndef _ASM_RISCV_PGTABLE_32_H
#define _ASM_RISCV_PGTABLE_32_H

#include <asm-generic/pgtable-nopmd.h>

#define PGDIR_SHIFT     33
#define PGDIR_SIZE      (1UL << PGDIR_SHIFT)
#define PGDIR_MASK      (~(PGDIR_SIZE - 1))

#endif /* _ASM_RISCV_PGTABLE_32_H */
