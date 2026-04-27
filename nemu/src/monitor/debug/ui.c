#include "monitor/monitor.h"
#include "monitor/expr.h"
#include "monitor/watchpoint.h"
#include "nemu.h"

#include <stdlib.h>
#include <readline/readline.h>
#include <readline/history.h>

void cpu_exec(uint64_t);

/* We use the `readline' library to provide more flexibility to read from stdin. */
char *rl_gets()
{
  static char *line_read = NULL;

  if (line_read)
  {
    free(line_read);
    line_read = NULL;
  }

  line_read = readline("(nemu) ");

  if (line_read && *line_read)
  {
    add_history(line_read);
  }

  return line_read;
}

static int cmd_c(char *args)
{
  cpu_exec(-1);
  return 0;
}

static int cmd_q(char *args)
{
  return -1;
}

static int cmd_si(char *args)
{
  uint64_t n = 1;

  if (args != NULL)
  {
    char *endptr = NULL;
    n = strtoul(args, &endptr, 0);
    if (endptr == args)
    {
      printf("Usage: si [N]\n");
      return 0;
    }
  }

  cpu_exec(n);
  return 0;
}

static void print_regs(void)
{
  int i;
  for (i = R_EAX; i <= R_EDI; i++)
  {
    printf("%s\t0x%08x\n", regsl[i], reg_l(i));
  }
  printf("eip\t0x%08x\n", cpu.eip);

  for (i = R_AX; i <= R_DI; i++)
  {
    printf("%s\t0x%04x\n", regsw[i], reg_w(i));
  }

  for (i = R_AL; i <= R_BH; i++)
  {
    printf("%s\t0x%02x\n", regsb[i], reg_b(i));
  }
}

static int cmd_info(char *args)
{
  if (args == NULL)
  {
    printf("Usage: info r\n");
    return 0;
  }

  char *subcmd = strtok(args, " ");
  if (subcmd != NULL && strcmp(subcmd, "r") == 0)
  {
    print_regs();
    return 0;
  }

  if (subcmd != NULL && strcmp(subcmd, "w") == 0)
  {
    printf("Watchpoints are not implemented in phase 1.\n");
    return 0;
  }

  printf("Unknown info command '%s'\n", subcmd == NULL ? "" : subcmd);
  return 0;
}

static int cmd_x(char *args)
{
  if (args == NULL)
  {
    printf("Usage: x N EXPR\n");
    return 0;
  }

  char *n_str = strtok(args, " ");
  char *expr_str = strtok(NULL, " ");

  if (n_str == NULL || expr_str == NULL)
  {
    printf("Usage: x N EXPR\n");
    return 0;
  }

  char *n_end = NULL;
  unsigned long count = strtoul(n_str, &n_end, 10);
  if (n_end == n_str || *n_end != '\0')
  {
    printf("Invalid N: %s\n", n_str);
    return 0;
  }

  if (!(expr_str[0] == '0' && (expr_str[1] == 'x' || expr_str[1] == 'X')))
  {
    printf("For PA1 stage 1, EXPR only supports hex literal (e.g. 0x100000).\n");
    return 0;
  }

  char *expr_end = NULL;
  vaddr_t addr = (vaddr_t)strtoul(expr_str, &expr_end, 16);
  if (expr_end == expr_str || *expr_end != '\0')
  {
    printf("Invalid EXPR: %s\n", expr_str);
    return 0;
  }

  uint32_t i;
  for (i = 0; i < count; i++)
  {
    vaddr_t cur = addr + i * 4;
    uint32_t data = vaddr_read(cur, 4);
    printf("0x%08x: 0x%08x\n", cur, data);
  }

  return 0;
}

static int cmd_help(char *args);

static struct
{
  char *name;
  char *description;
  int (*handler)(char *);
} cmd_table[] = {
    {"help", "Display informations about all supported commands", cmd_help},
    {"c", "Continue the execution of the program", cmd_c},
    {"si", "Single step execution", cmd_si},
    {"info", "Print register information", cmd_info},
    {"x", "Scan memory", cmd_x},
    {"q", "Exit NEMU", cmd_q},

    /* TODO: Add more commands */

};

#define NR_CMD (sizeof(cmd_table) / sizeof(cmd_table[0]))

static int cmd_help(char *args)
{
  /* extract the first argument */
  char *arg = strtok(NULL, " ");
  int i;

  if (arg == NULL)
  {
    /* no argument given */
    for (i = 0; i < NR_CMD; i++)
    {
      printf("%s - %s\n", cmd_table[i].name, cmd_table[i].description);
    }
  }
  else
  {
    for (i = 0; i < NR_CMD; i++)
    {
      if (strcmp(arg, cmd_table[i].name) == 0)
      {
        printf("%s - %s\n", cmd_table[i].name, cmd_table[i].description);
        return 0;
      }
    }
    printf("Unknown command '%s'\n", arg);
  }
  return 0;
}

void ui_mainloop(int is_batch_mode)
{
  if (is_batch_mode)
  {
    cmd_c(NULL);
    return;
  }

  while (1)
  {
    char *str = rl_gets();
    char *str_end = str + strlen(str);

    /* extract the first token as the command */
    char *cmd = strtok(str, " ");
    if (cmd == NULL)
    {
      continue;
    }

    /* treat the remaining string as the arguments,
     * which may need further parsing
     */
    char *args = cmd + strlen(cmd) + 1;
    if (args >= str_end)
    {
      args = NULL;
    }

#ifdef HAS_IOE
    extern void sdl_clear_event_queue(void);
    sdl_clear_event_queue();
#endif

    int i;
    for (i = 0; i < NR_CMD; i++)
    {
      if (strcmp(cmd, cmd_table[i].name) == 0)
      {
        if (cmd_table[i].handler(args) < 0)
        {
          return;
        }
        break;
      }
    }

    if (i == NR_CMD)
    {
      printf("Unknown command '%s'\n", cmd);
    }
  }
}
