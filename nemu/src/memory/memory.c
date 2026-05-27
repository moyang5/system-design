#include "nemu.h"
#include "device/mmio.h"

#define PMEM_SIZE (128 * 1024 * 1024)

#define pmem_rw(addr, type) *(type *)({                                       \
  Assert(addr < PMEM_SIZE, "physical address(0x%08x) is out of bound", addr); \
  guest_to_host(addr);                                                        \
})

uint8_t pmem[PMEM_SIZE];

/* Memory accessing interfaces */

static inline bool paging_enabled(void)
{
  return cpu.cr0.protect_enable && cpu.cr0.paging;
}

#define PDX(va)     (((uint32_t)(va) >> 22) & 0x3ff)
#define PTX(va)     (((uint32_t)(va) >> 12) & 0x3ff)
#define OFF(va)     ((uint32_t)(va) & 0xfff)
#define PTE_ADDR(pte)    ((uint32_t)(pte) & ~0xfff)

static inline paddr_t page_translate(vaddr_t addr, bool write)
{
  PDE pde, *pgdir;
  PTE pte, *ptdir;
  if (cpu.cr0.protect_enable && cpu.cr0.paging)
  {
    pgdir = (PDE *)(PTE_ADDR(cpu.cr3.val));
    pde.val = paddr_read((paddr_t)&pgdir[PDX(addr)], 4);
    Assert(pde.present, "PDE not present: vaddr=0x%08x pde.val=0x%08x", addr, pde.val);
    pde.accessed = 1;
    ptdir = (PTE *)(PTE_ADDR(pde.val));
    pte.val = paddr_read((paddr_t)&ptdir[PTX(addr)], 4);
    Assert(pte.present, "ptdir:%p, pte.val: 0x%x, addr: 0x%x", ptdir, pte.val, addr);
    pte.accessed = 1;
    pte.dirty = write ? 1 : pte.dirty;
    return PTE_ADDR(pte.val) | OFF(addr);
  }
  return addr;
}

uint32_t paddr_read(paddr_t addr, int len)
{
  int map_NO = is_mmio(addr);
  if (map_NO != -1)
  {
    return mmio_read(addr, len, map_NO);
  }
  return pmem_rw(addr, uint32_t) & (~0u >> ((4 - len) << 3));
}

void paddr_write(paddr_t addr, int len, uint32_t data)
{
  int map_NO = is_mmio(addr);
  if (map_NO != -1)
  {
    mmio_write(addr, len, data, map_NO);
    return;
  }
  memcpy(guest_to_host(addr), &data, len);
}

uint32_t vaddr_read(vaddr_t addr, int len)
{
  if ((addr & PAGE_MASK) + len > PAGE_SIZE)
  {
    int len1 = PAGE_SIZE - (addr & PAGE_MASK);
    int len2 = len - len1;

    paddr_t paddr1 = paging_enabled() ? page_translate(addr, false) : addr;
    uint32_t low = paddr_read(paddr1, len1);

    vaddr_t addr2 = addr + len1;
    paddr_t paddr2 = paging_enabled() ? page_translate(addr2, false) : addr2;
    uint32_t high = paddr_read(paddr2, len2);

    return low | (high << (len1 * 8));
  }

  paddr_t paddr = paging_enabled() ? page_translate(addr, false) : addr;
  return paddr_read(paddr, len);
}

void vaddr_write(vaddr_t addr, int len, uint32_t data)
{
  if ((addr & PAGE_MASK) + len > PAGE_SIZE)
  {
    int len1 = PAGE_SIZE - (addr & PAGE_MASK);
    int len2 = len - len1;

    paddr_t paddr1 = paging_enabled() ? page_translate(addr, true) : addr;
    uint32_t low = data & (~0u >> ((4 - len1) << 3));
    paddr_write(paddr1, len1, low);

    vaddr_t addr2 = addr + len1;
    paddr_t paddr2 = paging_enabled() ? page_translate(addr2, true) : addr2;
    uint32_t high = data >> (len1 * 8);
    paddr_write(paddr2, len2, high);
    return;
  }

  paddr_t paddr = paging_enabled() ? page_translate(addr, true) : addr;
  paddr_write(paddr, len, data);
}
