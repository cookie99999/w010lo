/*
  translated from my 65816 code which is in turn translated
  from ken shirriff's bcpl code for the xerox alto
*/

#include <stdint.h>
#include <stddef.h>

const uint8_t colors[21] = {
  15, 1, 2, 3, 4, 5, 6, 7, 8, 9, 10, 11, 12, 13, 14, 1, 2, 3, 4, 5, 0
};

uint16_t mul16fix(uint16_t a, uint16_t b) {
  uint16_t a_bak = a;
  uint16_t b_bak = b;
  if (a & 0x8000)
    a ^= 0xffff;
  if (b & 0x8000)
    b ^= 0xffff;

  uint32_t result = a * b;
  result >>= 12;

  if (a_bak & 0x8000) {
    if (b_bak & 0x8000) {
      // neg x neg = pos do nothing
    } else {
      result ^= 0xffff;
    }
  } else if (b_bak & 0x8000) {
    result ^= 0xffff;
  }

  return (uint16_t)result;
}

int main() {
  const uint16_t x0 = 0xe000;
  const uint16_t x1 = 0x1000;
  const uint16_t y0 = 0xf000;
  const uint16_t y1 = 0x1000;
  const uint16_t xstep = 0x0060;
  const uint16_t ystep = 0x0055;

  uint16_t cx, cy, xv, yv, x2, y2, xy;

  *((volatile uint8_t *)0xfa3001) = 0x03;
  *((volatile uint8_t *)0xfa3001) = 0x0e;

  cy = y0;

  for (int i = 96; i > 0; i--) {
    cx = x0;
    for (int j = 128; j > 0; j--) {
      xv = cx;
      yv = cy;
      int n;
      for (n = 0; n < 20; n++) {
	y2 = mul16fix(yv, yv);
	x2 = mul16fix(xv, xv);
	if (x2 + y2 <= 0x4000) {
	  xy = mul16fix(xv, yv);
	  yv = xy + xy + cy;
	  xv = (x2 - y2) + cx;
	} else {
	  break;
	}
      }
      *((volatile uint8_t *)0xfa3001) = colors[n] | 0x80;
      cx += xstep;
    }
    cy += ystep;
  }
  *((volatile uint8_t *)0xfa3001) = 0x01;
}
