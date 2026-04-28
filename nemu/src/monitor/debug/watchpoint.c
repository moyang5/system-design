#include "monitor/watchpoint.h"
#include "monitor/expr.h"
#include "monitor/monitor.h"

#include <stdio.h>
#include <string.h>

#define NR_WP 32
#define WP_EXPR_LEN 128

static WP wp_pool[NR_WP];
static WP *head, *free_;

static char wp_expr[NR_WP][WP_EXPR_LEN];
static uint32_t wp_value[NR_WP];

void init_wp_pool()
{
  int i;
  for (i = 0; i < NR_WP; i++)
  {
    wp_pool[i].NO = i;
    wp_pool[i].next = &wp_pool[i + 1];
    wp_expr[i][0] = '\0';
    wp_value[i] = 0;
  }
  wp_pool[NR_WP - 1].next = NULL;

  head = NULL;
  free_ = wp_pool;
}

static WP *new_wp(void)
{
  if (free_ == NULL)
  {
    printf("No free watchpoint.\n");
    return NULL;
  }
  WP *wp = free_;
  free_ = free_->next;
  wp->next = head;
  head = wp;
  return wp;
}

static void free_wp(WP *wp)
{
  wp->next = free_;
  free_ = wp;
}

bool wp_add(const char *expr_str)
{
  WP *wp = new_wp();
  if (wp == NULL)
  {
    return false;
  }

  strncpy(wp_expr[wp->NO], expr_str, WP_EXPR_LEN - 1);
  wp_expr[wp->NO][WP_EXPR_LEN - 1] = '\0';

  bool success = false;
  uint32_t value = expr(wp_expr[wp->NO], &success);
  if (!success)
  {
    printf("Bad expression: %s\n", wp_expr[wp->NO]);
    head = wp->next;
    free_wp(wp);
    return false;
  }

  wp_value[wp->NO] = value;
  printf("Watchpoint %d set on %s = 0x%08x\n", wp->NO, wp_expr[wp->NO], value);
  return true;
}

bool wp_delete(int no)
{
  WP *prev = NULL;
  WP *cur = head;
  while (cur != NULL)
  {
    if (cur->NO == no)
    {
      if (prev == NULL)
      {
        head = cur->next;
      }
      else
      {
        prev->next = cur->next;
      }
      free_wp(cur);
      return true;
    }
    prev = cur;
    cur = cur->next;
  }
  return false;
}

bool wp_check(void)
{
  WP *cur = head;
  while (cur != NULL)
  {
    bool success = false;
    uint32_t new_value = expr(wp_expr[cur->NO], &success);
    if (!success)
    {
      printf("Bad expression: %s\n", wp_expr[cur->NO]);
      nemu_state = NEMU_STOP;
      return true;
    }
    if (new_value != wp_value[cur->NO])
    {
      printf("Watchpoint %d hit: %s\n", cur->NO, wp_expr[cur->NO]);
      printf("Old value = 0x%08x\n", wp_value[cur->NO]);
      printf("New value = 0x%08x\n", new_value);
      wp_value[cur->NO] = new_value;
      nemu_state = NEMU_STOP;
      return true;
    }
    cur = cur->next;
  }
  return false;
}

void wp_list(void)
{
  WP *cur = head;
  if (cur == NULL)
  {
    printf("No watchpoints.\n");
    return;
  }

  printf("No\tExpr\tValue\n");
  while (cur != NULL)
  {
    printf("%d\t%s\t0x%08x\n", cur->NO, wp_expr[cur->NO], wp_value[cur->NO]);
    cur = cur->next;
  }
}
