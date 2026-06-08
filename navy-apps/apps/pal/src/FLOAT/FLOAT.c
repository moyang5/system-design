#include "FLOAT.h"
#include <stdint.h>
#include <assert.h>

static int32_t fixed_div(int32_t a, int32_t b)
{
  assert(b != 0);

  uint32_t ua = (a < 0) ? (uint32_t)(-a) : (uint32_t)a;
  uint32_t ub = (b < 0) ? (uint32_t)(-b) : (uint32_t)b;

  uint32_t q = ua / ub;
  uint32_t r = ua % ub;
  uint32_t frac = 0;

  for (int i = 0; i < 16; ++i)
  {
    r <<= 1;
    frac <<= 1;
    if (r >= ub)
    {
      r -= ub;
      frac |= 1u;
    }
  }

  int32_t result = (int32_t)((q << 16) | frac);
  if ((a < 0) ^ (b < 0))
  {
    result = -result;
  }

  return result;
}

static int32_t fixed_mul(int32_t a, int32_t b)
{
  int sign = 0;
  uint32_t ua = (uint32_t)a;
  uint32_t ub = (uint32_t)b;

  if (a < 0)
  {
    ua = (uint32_t)(-a);
    sign ^= 1;
  }

  if (b < 0)
  {
    ub = (uint32_t)(-b);
    sign ^= 1;
  }

  uint32_t ah = ua >> 16;
  uint32_t al = ua & 0xFFFFu;
  uint32_t bh = ub >> 16;
  uint32_t bl = ub & 0xFFFFu;

  uint32_t mid = ah * bl + al * bh;
  uint32_t low = al * bl;
  uint32_t high = ah * bh;

  uint32_t res = (high << 16) + mid + (low >> 16);
  int32_t result = (int32_t)res;

  if (sign)
  {
    result = -result;
  }

  return result;
}

FLOAT F_mul_F(FLOAT a, FLOAT b)
{
  return fixed_mul(a, b);
}

FLOAT F_div_F(FLOAT a, FLOAT b)
{
  return fixed_div(a, b);
}

FLOAT f2F(float a)
{
  /* You should figure out how to convert `a' into FLOAT without
   * introducing x87 floating point instructions. Else you can
   * not run this code in NEMU before implementing x87 floating
   * point instructions, which is contrary to our expectation.
   *
   * Hint: The bit representation of `a' is already on the
   * stack. How do you retrieve it to another variable without
   * performing arithmetic operations on it directly?
   */

  union
  {
    float f;
    uint32_t u;
  } v;

  v.f = a;

  uint32_t sign = v.u >> 31;
  uint32_t exp = (v.u >> 23) & 0xFF;
  uint32_t frac = v.u & 0x7FFFFF;

  if (exp == 0)
  {
    return 0;
  }

  if (exp == 0xFF)
  {
    return sign ? (FLOAT)0x80000000 : (FLOAT)0x7FFFFFFF;
  }

  uint32_t mantissa = (1u << 23) | frac;
  int shift = (int)exp - 134;
  uint32_t value = mantissa;

  if (shift >= 0)
  {
    for (int i = 0; i < shift; ++i)
    {
      value <<= 1;
    }
  }
  else
  {
    for (int i = 0; i < -shift; ++i)
    {
      value >>= 1;
    }
  }

  if (sign)
  {
    return -(FLOAT)value;
  }

  return (FLOAT)value;
}

FLOAT Fabs(FLOAT a)
{
  return (a < 0) ? -a : a;
}

/* Functions below are already implemented */

FLOAT Fsqrt(FLOAT x)
{
  FLOAT dt, t = int2F(2);

  do
  {
    dt = F_div_int((F_div_F(x, t) - t), 2);
    t += dt;
  } while (Fabs(dt) > f2F(1e-4));

  return t;
}

FLOAT Fpow(FLOAT x, FLOAT y)
{
  /* we only compute x^0.333 */
  FLOAT t2, dt, t = int2F(2);

  do
  {
    t2 = F_mul_F(t, t);
    dt = (F_div_F(x, t2) - t) / 3;
    t += dt;
  } while (Fabs(dt) > f2F(1e-4));

  return t;
}
