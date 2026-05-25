#include "common.h"
#include "syscall.h"

_RegSet *do_syscall(_RegSet *r)
{
  uintptr_t a[4];
  a[0] = SYSCALL_ARG1(r);
  a[1] = SYSCALL_ARG2(r);
  a[2] = SYSCALL_ARG3(r);
  a[3] = SYSCALL_ARG4(r);
  uintptr_t ret = 0;

  switch (a[0])
  {
  case SYS_none:
    ret = 1;
    break;
  case SYS_write:
  {
    int fd = (int)a[1];
    const char *buf = (const char *)a[2];
    size_t len = (size_t)a[3];
    if (fd == 1 || fd == 2)
    {
      for (size_t i = 0; i < len; i++)
      {
        _putc(buf[i]);
      }
      Log("sys_write:fd %d len %d", fd, (int)len);
      ret = len;
    }
    else
    {
      ret = (uintptr_t)-1;
    }
    break;
  }
  case SYS_exit:
    _halt((int)a[1]);
    break;
  default:
    panic("Unhandled syscall ID = %d", a[0]);
  }

  r->eax = ret;
  return r;
}
