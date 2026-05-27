#include "common.h"
#include "fs.h"
#include "memory.h"

#define DEFAULT_ENTRY ((void *)0x8048000)

uintptr_t loader(_Protect *as, const char *filename)
{
  int fd = fs_open(filename, 0, 0);
  int size = fs_filesz(fd);
  void *pa, *va = DEFAULT_ENTRY;
  Log("filename=%s, fd=%d", filename, fd);
  while (size > 0)
  {
    pa = new_page();
    _map(as, va, pa);
    size_t len = size >= PGSIZE ? PGSIZE : size;
    fs_read(fd, pa, len);
    va += PGSIZE;
    size -= PGSIZE;
  }
  fs_close(fd);
  return (uintptr_t)DEFAULT_ENTRY;
}
