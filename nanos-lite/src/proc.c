#include "proc.h"

#define MAX_NR_PROC 4

static PCB pcb[MAX_NR_PROC];
static int nr_proc = 0;
PCB *current = NULL;
int count = 0;

uintptr_t loader(_Protect *as, const char *filename);

void load_prog(const char *filename)
{
  int i = nr_proc++;
  _protect(&pcb[i].as);

  uintptr_t entry = loader(&pcb[i].as, filename);

  _Area stack;
  stack.start = pcb[i].stack;
  stack.end = stack.start + sizeof(pcb[i].stack);

  pcb[i].tf = _umake(&pcb[i].as, stack, stack, (void *)entry, NULL, NULL);
}

// PAL(仙剑)和hello的调度频率比例
// 每SCHED_RATIO次调度中,hello只运行1次,其余时间运行PAL
#define SCHED_RATIO 1000

_RegSet *schedule(_RegSet *prev)
{
  if (current == NULL)
  {
    current = &pcb[0];
  }
  else
  {
    current->tf = prev;
    count++;
    if (count % SCHED_RATIO == 0)
    {
      current = &pcb[1];  // hello
    }
    else
    {
      current = &pcb[0];  // PAL
    }
  }
  _switch(&current->as);
  return current->tf;
}
