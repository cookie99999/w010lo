#define MFP_BASE 0x080000
#define MFP_GPIP MFP_BASE+1
#define MFP_AER MFP_BASE+3
#define MFP_DDR MFP_BASE+5
#define MFP_IERA MFP_BASE+7
#define MFP_IERB MFP_BASE+9
#define MFP_IPRA MFP_BASE+11
#define MFP_IPRB MFP_BASE+13
#define MFP_ISRA MFP_BASE+15
#define MFP_ISRB MFP_BASE+17
#define MFP_IMRA MFP_BASE+19
#define MFP_IMRB MFP_BASE+21
#define MFP_VR MFP_BASE+23
#define MFP_TACR MFP_BASE+25
#define MFP_TBCR MFP_BASE+27
#define MFP_TCDCR MFP_BASE+29
#define MFP_TADR MFP_BASE+31
#define MFP_TBDR MFP_BASE+33
#define MFP_TCDR MFP_BASE+35
#define MFP_TDDR MFP_BASE+37
#define MFP_SCR MFP_BASE+39
#define MFP_UCR MFP_BASE+41
#define MFP_RSR MFP_BASE+43
#define MFP_TSR MFP_BASE+45
#define MFP_UDR MFP_BASE+47

volatile unsigned char *linebuf = (volatile unsigned char*)0x000400;
unsigned char asc2byte(char hi, char lo);

void putch(char c) {
  asm volatile ("1: btst.b #2, 0x100003\n"
		"beq 1b\n"
		"move.b %0, 0x100007"
		:
		:"r"(c)
		:"%d0");
}

void putstr(const char * s) {
  while (*s != '\0') {
    putch(*s);
    s++;
  }
}

char getchar_b() {
  char c;
  asm volatile ("1: btst.b #0, 0x100003\n"
		"beq 1b\n"
		"move.b 0x100007, %%d0\n"
		"move.b %%d0, %0"
		:"=r"(c)
		:
		:"%d0");
  return c;
}

int is_digit(char c) {
  if (c >= '0' && c <= 'F' && (c < ':' || c > '@'))
    return 1;
  else
    return 0;
}

void prbyte(unsigned char b) {
  unsigned char hi = (b >> 4) & 0x0f;
  hi += 0x30;
  if (hi > '9')
    hi += 7;
  putch(hi);
  b = (b & 0x0f) + 0x30;
  if (b > '9')
    b += 7;
  putch(b);
}

void prlong(unsigned int l) {
  prbyte((unsigned char)(l >> 24));
  prbyte((unsigned char)(l >> 16));
  prbyte((unsigned char)(l >> 8));
  prbyte((unsigned char)(l & 0xff));
}

/*void to_upper() {
  volatile unsigned char *lbptr = linebuf;
  char c;
  while ((c = *lbptr) != 0) {
    if (c >= 'a' && c <= 'z')
      *lbptr = c - 0x20;
    lbptr++;
  }
}

void readline() {
  volatile unsigned char *lbptr = linebuf;
  char c;
  while (true) {
    while ((c = getchar_b()) != '\r') {
      if (c == 0x08) { //backspace
	if (lbptr != linebuf) { // dont allow bs on empty line
	  lbptr--;
	  putch(c);
	}
	continue;
      }
      if (c > 0x1f && c <= 0x7f) {
	putch(c);
	*lbptr++ = c;
      }	
    }
    if (lbptr != linebuf) { //more than zero chars in buffer
      *lbptr = '\0';
      putstr("\r\n");
      return;
    } // else loop
  }
}

void bad_input() {
  putstr("ERR: Bad input\r\n");
}

void do_run(volatile unsigned char *addr) {
  asm volatile ("movea.l %d0, %a1\njmp (%a1)\nrts");
}

void do_poke(volatile unsigned char **lbptr, volatile unsigned char *addr) {
  char c = **lbptr;
  *lbptr++;
  while (true) {
    if (c == ' ') {
      c = **lbptr;
      *lbptr++;
    }
    if (!is_digit(c)) {
      bad_input();
      return;
    }
    char d = **lbptr;
    *lbptr++;
    if (!is_digit(d)) {
      bad_input();
      return;
    }
    *addr++ = asc2byte(c, d);
    c = **lbptr;
    *lbptr++;
    if (c != ' ')
      return;
  }
}

int getaddr(volatile unsigned char **lbptr, volatile unsigned char **addr) {
  unsigned int a = 0;
  for (short i = 0; i < 4; i++) {
    char c = **lbptr;
    if (!is_digit(c))
      break;
    *lbptr++;
    char d = **lbptr;
    *lbptr++;
    if (!is_digit(d)) {
      bad_input();
      return 0;
    }
    a <<= 8;
    a |= (unsigned int)asc2byte(c, d);
  }
  *addr = (volatile unsigned char*)a;
  return 1;
}

void peek(volatile unsigned char *addr) {
  prlong((unsigned int)addr);
  putstr(": ");
  putch((char)*addr);
  putstr("\r\n");
}

void peek_range(volatile unsigned char *addr, volatile unsigned char *addr2) {
  prlong((unsigned int)addr);
  putstr(": ");
  do {
    for (short i = 0; i < 16; i++) {
      putch((char)*addr++);
      putch(' ');
      if (addr > addr2)
	break;
    }
    putstr("\r\n");
    if (addr > addr2)
      break;
    putstr("          ");
  } while (true);
}

void parseline() {
  volatile unsigned char *lbptr = linebuf;
  char c = *lbptr;
  if (!is_digit(c)) {
    bad_input();
    return;
  }
  volatile unsigned char *addr;
  if (!getaddr(&lbptr, &addr)) {
    bad_input();
    return;
  }

  c = *lbptr++;
  if (c == ':') {
    do_poke(&lbptr, addr);
    return;
  } else if (c == 'G') {
    do_run(addr);
    return;
  } else if (c == '.') {
    volatile unsigned char *addr2;
    if (!getaddr(&lbptr, &addr2)) {
      bad_input();
      return;
    }
    peek_range(addr, addr2);
    return;
  } else {
    peek(addr);
    return;
  }
  }*/

unsigned char asc2byte(char hi, char lo) {
  hi -= '0';
  if (hi > 9)
    hi -= 7;
  hi &= 0x0f;

  lo -= '0';
  if (lo > 9)
    lo -= 7;
  lo &= 0x0f;

  return (unsigned char)(hi << 4) | lo;
}

char chartab[] = {'.', '.', '.', ',', ',', ',',
		  ':', ':', ':', 'i', 'i', 'i',
		  'w', 'w', 'w', 'W', 'W', 'W',
		  '#', '#', ' '};

void mandel() {
  int x0 = (-2) << 12;
  int x1 = 1 << 12;
  int y0 = (-1) << 12;
  int y1 = 1 << 12;
  int xstep = (x1 - x0) / 80;
  int ystep = (y1 - y0) / 25;
  int x2, y2, xy;
  int cy = y0;

  for (int ypos = 0; ypos < 25; ypos++) {
    int cx = x0;
    for (int xpos = 0; xpos < 80; xpos++) {
      int x = cx;
      int y = cy;
      int n;
      for (n = 20; n > 0; n--) {
	y2 = y * y;
	x2 = x * x;
	y2 >>= 12;
	x2 >>= 12;
	if ((x2 + y2) >= 0x4000)
	  break;

	xy = x * y;
        xy /= 4096;

	y = xy + xy + cy;
	x = x2 - y2 + cx;
      }
      putch(chartab[n]);
      cx += xstep;
    }
    putstr("\r\n");
    cy += ystep;
  }
  return;
}

void main() {
  mandel();
  return;
  /*
  putstr("Ready\r\n");
  while (true) {
    putch(']');
    readline();
    to_upper();
    parseline();
    }*/
}
