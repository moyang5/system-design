#include "fs.h"

typedef struct
{
  char *name;
  size_t size;
  off_t disk_offset;
} Finfo;

enum
{
  FD_STDIN,
  FD_STDOUT,
  FD_STDERR,
  FD_FB,
  FD_EVENTS,
  FD_DISPINFO,
  FD_NORMAL
};

/* This is the information about all files in disk. */
static Finfo file_table[] __attribute__((used)) = {
    {"stdin (note that this is not the actual stdin)", 0, 0},
    {"stdout (note that this is not the actual stdout)", 0, 0},
    {"stderr (note that this is not the actual stderr)", 0, 0},
    [FD_FB] = {"/dev/fb", 0, 0},
    [FD_EVENTS] = {"/dev/events", 0, 0},
    [FD_DISPINFO] = {"/proc/dispinfo", 128, 0},
#include "files.h"
};

#define NR_FILES (sizeof(file_table) / sizeof(file_table[0]))

static size_t open_offset[NR_FILES];

extern size_t events_read(void *buf, size_t len);
extern void dispinfo_read(void *buf, off_t offset, size_t len);
extern void fb_write(const void *buf, off_t offset, size_t len);

void init_fs()
{
  file_table[FD_FB].size = _screen.width * _screen.height * 4;
}

int fs_open(const char *pathname, int flags, int mode)
{
  (void)flags;
  (void)mode;
  for (size_t i = 0; i < NR_FILES; i++)
  {
    if (strcmp(file_table[i].name, pathname) == 0)
    {
      open_offset[i] = 0;
      return (int)i;
    }
  }
  assert(0);
  return -1;
}

size_t fs_read(int fd, void *buf, size_t len)
{
  if (fd == FD_STDIN || fd == FD_STDOUT || fd == FD_STDERR)
  {
    return 0;
  }

  if (fd == FD_EVENTS)
  {
    return events_read(buf, len);
  }

  if (fd == FD_DISPINFO)
  {
    size_t size = file_table[fd].size;
    if (open_offset[fd] >= size)
    {
      return 0;
    }
    if (open_offset[fd] + len > size)
    {
      len = size - open_offset[fd];
    }
    dispinfo_read(buf, open_offset[fd], len);
    open_offset[fd] += len;
    return len;
  }

  size_t size = file_table[fd].size;
  if (open_offset[fd] >= size)
  {
    return 0;
  }
  if (open_offset[fd] + len > size)
  {
    len = size - open_offset[fd];
  }
  ramdisk_read(buf, file_table[fd].disk_offset + open_offset[fd], len);
  open_offset[fd] += len;
  return len;
}

size_t fs_write(int fd, const void *buf, size_t len)
{
  if (fd == FD_STDOUT || fd == FD_STDERR)
  {
    const char *p = (const char *)buf;
    for (size_t i = 0; i < len; i++)
    {
      _putc(p[i]);
    }
    return len;
  }

  if (fd == FD_STDIN)
  {
    return 0;
  }

  if (fd == FD_FB)
  {
    size_t size = file_table[fd].size;
    if (open_offset[fd] >= size)
    {
      return 0;
    }
    if (open_offset[fd] + len > size)
    {
      len = size - open_offset[fd];
    }
    fb_write(buf, open_offset[fd], len);
    open_offset[fd] += len;
    return len;
  }

  size_t size = file_table[fd].size;
  if (open_offset[fd] >= size)
  {
    return 0;
  }
  if (open_offset[fd] + len > size)
  {
    len = size - open_offset[fd];
  }
  ramdisk_write(buf, file_table[fd].disk_offset + open_offset[fd], len);
  open_offset[fd] += len;
  return len;
}

off_t fs_lseek(int fd, off_t offset, int whence)
{
  size_t size = file_table[fd].size;
  off_t base = 0;
  switch (whence)
  {
  case SEEK_SET:
    base = 0;
    break;
  case SEEK_CUR:
    base = (off_t)open_offset[fd];
    break;
  case SEEK_END:
    base = (off_t)size;
    break;
  default:
    assert(0);
  }

  off_t new_off = base + offset;
  assert(new_off >= 0);
  assert((size_t)new_off <= size);
  open_offset[fd] = (size_t)new_off;
  return new_off;
}

int fs_close(int fd)
{
  (void)fd;
  return 0;
}

size_t fs_filesz(int fd)
{
  return file_table[fd].size;
}
