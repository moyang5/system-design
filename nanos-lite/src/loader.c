#include "common.h"
#include "fs.h"
#include "memory.h"

#define DEFAULT_ENTRY ((void *)0x4000000)

uintptr_t loader(_Protect *as, const char *filename)
{
  if (filename == NULL)
  {
    filename = "/bin/hello";
  }

  int fd = fs_open(filename, 0, 0);
  size_t size = fs_filesz(fd);
  if (as == NULL)
  {
    size_t read_size = fs_read(fd, DEFAULT_ENTRY, size);
    assert(read_size == size);
    fs_close(fd);
    return (uintptr_t)DEFAULT_ENTRY;
  }

  size_t remain = size;
  uintptr_t va = (uintptr_t)DEFAULT_ENTRY;
  while (remain > 0)
  {
    void *pa = new_page();
    _map(as, (void *)va, pa);
    memset(pa, 0, PGSIZE);
    size_t chunk = remain > PGSIZE ? PGSIZE : remain;
    size_t read_size = fs_read(fd, pa, chunk);
    assert(read_size == chunk);
    va += PGSIZE;
    remain -= chunk;
  }
  fs_close(fd);
  return (uintptr_t)DEFAULT_ENTRY;
}
