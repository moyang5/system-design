#include "cpu/exec.h"
#include "memory/mmu.h"

void raise_intr(uint8_t NO, vaddr_t ret_addr)
{
  /* TODO: Trigger an interrupt/exception with ``NO''.
   * That is, use ``NO'' to index the IDT.
   */
  rtlreg_t t;

  t = cpu.eflags.val;
  rtl_push(&t);

  t = cpu.cs;
  rtl_push(&t);

  t = ret_addr;
  rtl_push(&t);

  vaddr_t desc_addr = cpu.idtr.base + NO * 8;
  uint32_t low = vaddr_read(desc_addr, 4);
  uint32_t high = vaddr_read(desc_addr + 4, 4);

  uint32_t offset = (low & 0x0000ffff) | (high & 0xffff0000);
  cpu.cs = (low >> 16) & 0xffff;

  decoding.is_jmp = 1;
  decoding.jmp_eip = offset;
}

void dev_raise_intr()
{
}
