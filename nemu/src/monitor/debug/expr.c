#include "nemu.h"

/* We use the POSIX regex functions to process regular expressions.
 * Type 'man regex' for more information about POSIX regex functions.
 */
#include <sys/types.h>
#include <regex.h>
#include <stdlib.h>
#include <string.h>

enum
{
  TK_NOTYPE = 256,
  TK_EQ,
  TK_NUMBER,
  TK_HEX,
  TK_REG,
  TK_NEQ,
  TK_AND,
  TK_OR,
  TK_NEGATIVE,
  TK_DEREF
};

static struct rule
{
  char *regex;
  int token_type;
} rules[] = {

    {" +", TK_NOTYPE}, // spaces
    {"0[xX][0-9a-fA-F]+", TK_HEX},
    {"[0-9]+", TK_NUMBER},
    {"\\$[a-zA-Z][a-zA-Z0-9]*", TK_REG},
    {"==", TK_EQ},
    {"!=", TK_NEQ},
    {"&&", TK_AND},
    {"\\|\\|", TK_OR},
    {"\\+", '+'},
    {"-", '-'},
    {"\\*", '*'},
    {"/", '/'},
    {"\\(", '('},
    {"\\)", ')'}};

#define NR_REGEX (sizeof(rules) / sizeof(rules[0]))

static regex_t re[NR_REGEX];

/* Rules are used for many times.
 * Therefore we compile them only once before any usage.
 */
void init_regex()
{
  int i;
  char error_msg[128];
  int ret;

  for (i = 0; i < NR_REGEX; i++)
  {
    ret = regcomp(&re[i], rules[i].regex, REG_EXTENDED);
    if (ret != 0)
    {
      regerror(ret, &re[i], error_msg, 128);
      panic("regex compilation failed: %s\n%s", error_msg, rules[i].regex);
    }
  }
}

typedef struct token
{
  int type;
  char str[32];
} Token;

Token tokens[32];
int nr_token;

static bool is_unary_context(int type)
{
  switch (type)
  {
  case TK_EQ:
  case TK_NEQ:
  case TK_AND:
  case TK_OR:
  case '+':
  case '-':
  case '*':
  case '/':
  case '(':
  case TK_NEGATIVE:
  case TK_DEREF:
    return true;
  default:
    return false;
  }
}

static bool check_parentheses(int l, int r)
{
  if (tokens[l].type != '(' || tokens[r].type != ')')
  {
    return false;
  }

  int depth = 0;
  int i;
  for (i = l; i <= r; i++)
  {
    if (tokens[i].type == '(')
    {
      depth++;
    }
    else if (tokens[i].type == ')')
    {
      depth--;
      if (depth == 0 && i < r)
      {
        return false;
      }
      if (depth < 0)
      {
        return false;
      }
    }
  }

  return depth == 0;
}

static int precedence(int type)
{
  switch (type)
  {
  case TK_OR:
    return 1;
  case TK_AND:
    return 2;
  case TK_EQ:
  case TK_NEQ:
    return 3;
  case '+':
  case '-':
    return 4;
  case '*':
  case '/':
    return 5;
  default:
    return 0;
  }
}

static bool is_binary_operator(int type)
{
  switch (type)
  {
  case TK_OR:
  case TK_AND:
  case TK_EQ:
  case TK_NEQ:
  case '+':
  case '-':
  case '*':
  case '/':
    return true;
  default:
    return false;
  }
}

static uint32_t eval(int l, int r, bool *success)
{
  if (l > r)
  {
    *success = false;
    return 0;
  }

  if (l == r)
  {
    switch (tokens[l].type)
    {
    case TK_NUMBER:
      return (uint32_t)strtoul(tokens[l].str, NULL, 10);
    case TK_HEX:
      return (uint32_t)strtoul(tokens[l].str, NULL, 16);
    case TK_REG:
    {
      const char *name = tokens[l].str + 1;
      int i;
      for (i = R_EAX; i <= R_EDI; i++)
      {
        if (strcmp(name, regsl[i]) == 0)
        {
          return reg_l(i);
        }
      }
      for (i = R_AX; i <= R_DI; i++)
      {
        if (strcmp(name, regsw[i]) == 0)
        {
          return reg_w(i);
        }
      }
      for (i = R_AL; i <= R_BH; i++)
      {
        if (strcmp(name, regsb[i]) == 0)
        {
          return reg_b(i);
        }
      }
      if (strcmp(name, "eip") == 0)
      {
        return cpu.eip;
      }
      *success = false;
      return 0;
    }
    default:
      *success = false;
      return 0;
    }
  }

  if (check_parentheses(l, r))
  {
    return eval(l + 1, r - 1, success);
  }

  int main_op = -1;
  int main_prec = 0;
  int depth = 0;
  int i;
  for (i = l; i <= r; i++)
  {
    int type = tokens[i].type;
    if (type == '(')
    {
      depth++;
      continue;
    }
    if (type == ')')
    {
      depth--;
      continue;
    }
    if (depth > 0)
    {
      continue;
    }
    if (!is_binary_operator(type))
    {
      continue;
    }
    int prec = precedence(type);
    if (main_op == -1 || prec <= main_prec)
    {
      main_op = i;
      main_prec = prec;
    }
  }

  if (main_op == -1)
  {
    if (tokens[l].type == TK_NEGATIVE)
    {
      uint32_t val = eval(l + 1, r, success);
      return (uint32_t)(0 - val);
    }
    if (tokens[l].type == TK_DEREF)
    {
      uint32_t addr = eval(l + 1, r, success);
      return vaddr_read(addr, 4);
    }
    *success = false;
    return 0;
  }

  uint32_t left = eval(l, main_op - 1, success);
  if (!*success)
  {
    return 0;
  }
  uint32_t right = eval(main_op + 1, r, success);
  if (!*success)
  {
    return 0;
  }

  switch (tokens[main_op].type)
  {
  case TK_OR:
    return (left || right) ? 1 : 0;
  case TK_AND:
    return (left && right) ? 1 : 0;
  case TK_EQ:
    return (left == right) ? 1 : 0;
  case TK_NEQ:
    return (left != right) ? 1 : 0;
  case '+':
    return left + right;
  case '-':
    return left - right;
  case '*':
    return left * right;
  case '/':
    if (right == 0)
    {
      *success = false;
      return 0;
    }
    return left / right;
  default:
    *success = false;
    return 0;
  }
}

static bool make_token(char *e)
{
  int position = 0;
  int i;
  regmatch_t pmatch;

  nr_token = 0;

  while (e[position] != '\0')
  {
    /* Try all rules one by one. */
    for (i = 0; i < NR_REGEX; i++)
    {
      if (regexec(&re[i], e + position, 1, &pmatch, 0) == 0 && pmatch.rm_so == 0)
      {
        char *substr_start = e + position;
        int substr_len = pmatch.rm_eo;

        Log("match rules[%d] = \"%s\" at position %d with len %d: %.*s",
            i, rules[i].regex, position, substr_len, substr_len, substr_start);
        position += substr_len;

        /* TODO: Now a new token is recognized with rules[i]. Add codes
         * to record the token in the array `tokens'. For certain types
         * of tokens, some extra actions should be performed.
         */

        switch (rules[i].token_type)
        {
        case TK_NOTYPE:
          break;
        default:
          if (nr_token >= (int)(sizeof(tokens) / sizeof(tokens[0])))
          {
            printf("Too many tokens\n");
            return false;
          }
          tokens[nr_token].type = rules[i].token_type;
          if (substr_len >= (int)sizeof(tokens[nr_token].str))
          {
            printf("Token too long\n");
            return false;
          }
          strncpy(tokens[nr_token].str, substr_start, substr_len);
          tokens[nr_token].str[substr_len] = '\0';
          if (tokens[nr_token].type == '-' || tokens[nr_token].type == '*')
          {
            int prev_type = (nr_token == 0) ? 0 : tokens[nr_token - 1].type;
            if (nr_token == 0 || is_unary_context(prev_type))
            {
              tokens[nr_token].type = (tokens[nr_token].type == '-') ? TK_NEGATIVE : TK_DEREF;
            }
          }
          nr_token++;
          break;
        }

        break;
      }
    }

    if (i == NR_REGEX)
    {
      printf("no match at position %d\n%s\n%*.s^\n", position, e, position, "");
      return false;
    }
  }

  return true;
}

uint32_t expr(char *e, bool *success)
{
  if (!make_token(e))
  {
    *success = false;
    return 0;
  }

  *success = true;
  if (nr_token == 0)
  {
    *success = false;
    return 0;
  }

  return eval(0, nr_token - 1, success);

  return 0;
}
