#include "cpu/rtl.h"

/* Condition Code */

void rtl_setcc(rtlreg_t* dest, uint8_t subcode) {
  bool invert = subcode & 0x1;
  enum {
    CC_O, CC_NO, CC_B,  CC_NB,
    CC_E, CC_NE, CC_BE, CC_NBE,
    CC_S, CC_NS, CC_P,  CC_NP,
    CC_L, CC_NL, CC_LE, CC_NLE
  };

  switch (subcode & 0xe) {
    case CC_O:
      rtl_li(dest, cpu.flags.OF ? 1 : 0);
      break;
    case CC_B:
      rtl_li(dest, cpu.flags.CF ? 1 : 0);
      break;
    case CC_E:
      rtl_li(dest, cpu.flags.ZF ? 1 : 0);
      break;
    case CC_BE:
      rtl_li(dest, (cpu.flags.CF || cpu.flags.ZF) ? 1 : 0);
      break;
    case CC_S:
      rtl_li(dest, cpu.flags.SF ? 1 : 0);
      break;
    case CC_L:
      rtl_li(dest, (cpu.flags.OF != cpu.flags.SF) ? 1 : 0);
      break;
    case CC_LE:
      rtl_li(dest, (cpu.flags.OF != cpu.flags.SF || cpu.flags.ZF) ? 1 : 0);
      break;
    default: panic("should not reach here");
    case CC_P: panic("n86 does not have PF");
  }

  if (invert) {
    rtl_xori(dest, dest, 0x1);
  }
}
