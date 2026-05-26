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

static inline paddr_t page_translate(vaddr_t addr, bool is_write)
{
  uint32_t dir_idx = addr >> 22;
  uint32_t tbl_idx = (addr >> 12) & 0x3ff;
  uint32_t offset = addr & PAGE_MASK;

  paddr_t pdir_base = cpu.cr3.page_directory_base << 12;
  paddr_t pde_addr = pdir_base + dir_idx * 4;
  PDE pde;
  pde.val = paddr_read(pde_addr, 4);
  assert(pde.present);
  if (!pde.accessed)
  {
    pde.accessed = 1;
    paddr_write(pde_addr, 4, pde.val);
  }

  paddr_t pte_base = pde.page_frame << 12;
  paddr_t pte_addr = pte_base + tbl_idx * 4;
  PTE pte;
  pte.val = paddr_read(pte_addr, 4);
  assert(pte.present);
  bool pte_changed = false;
  if (!pte.accessed)
  {
    pte.accessed = 1;
    pte_changed = true;
  }
  if (is_write && !pte.dirty)
  {
    pte.dirty = 1;
    pte_changed = true;
  }
  if (pte_changed)
  {
    paddr_write(pte_addr, 4, pte.val);
  }

  return (pte.page_frame << 12) | offset;
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
