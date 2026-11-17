#include <stdio.h>
#include <stdint.h>
#include <math.h>
#include "pico/stdlib.h"
#include "hardware/gpio.h"
#include "hardware/pio.h"
#include "hardware/clocks.h"
#include "hardware/structs/bus_ctrl.h"

#include "dbus.pio.h"
#include "vga_graphics.h"

#define D0 6
#define D1 7
#define D2 8
#define D3 9
#define D4 10
#define D5 11
#define D6 12
#define D7 13

#define RWB 0
#define CSB 1
#define DS 26
#define DTACK 27
#define RESB 22

static uint8_t dbus_buf[5120];
static uint8_t *dbus_bufend = dbus_buf + sizeof(dbus_buf);
volatile static uint8_t *dbus_rptr = dbus_buf;
volatile static uint8_t *dbus_wptr = dbus_buf;
static PIO dbus_pio;
static uint dbus_sm;
static uint dbus_offset;
static uint dbus_pio_irq;

volatile static bool cursor_state = false;
extern unsigned char *framebuf;
#define FBUF_SIZE 153600 * 2;
volatile uint32_t fbuf_addr = 0;
volatile bool lores = false;
volatile struct repeating_timer timer;
extern char textcolor;
extern char textbgcolor;
extern unsigned short textsize;
extern unsigned short cursor_x;
extern unsigned short cursor_y;
#define WIDTH 640
#define HEIGHT 480

enum states {
  TEXT_MODE,
  BITMAP_MODE,
  TILEMAP_MODE
};
volatile static char cur_state = TEXT_MODE;

enum cmds {
  DRAW_PIXEL = 0, GO_TEXT, GO_TILE, GO_BITMAP, CLEAR, DRAW_TRI, RES0,
  RES1, BACKSPACE, RES2, LF, RES3, RES4, CR, GO_LORES, GO_HIRES,
  SET_ADDR = 0x7e, WRITE_RAW, DRAW_PIX_SEQ = 0x80,
  RESET = 0xff
};

static void __not_in_flash_func(read_dbus)(void) {
  *dbus_wptr++ = dbus_pio->rxf[dbus_sm] >> 24;
  dbus_pio->irq = (1u << 0);
  if (dbus_wptr >= dbus_bufend)
    dbus_wptr = dbus_buf;
}

__attribute__((optimize("O0"))) bool get_dbus_byte(uint8_t *byte) {
  if (dbus_rptr != dbus_wptr) {
    *byte = *dbus_rptr++;
    if (dbus_rptr >= dbus_bufend)
      dbus_rptr = dbus_buf;
    return true;
  }
  return false;
}

__attribute__((optimize("O0"))) uint8_t get_dbus_byte_blocking() {
  while (dbus_rptr == dbus_wptr) {
    ;
  }
  uint8_t byte;
  byte = *dbus_rptr++;
  if (dbus_rptr >= dbus_bufend)
    dbus_rptr = dbus_buf;
  return byte;
}

bool toggle_cursor(__unused struct repeating_timer *t) {
  if (cursor_state) {
    fill_rect(cursor_x, cursor_y, 6, 8, textcolor);
  } else {
    fill_rect(cursor_x, cursor_y, 6, 8, textbgcolor);
  }
  cursor_state = !cursor_state;
  return true;
}

void vga_reset() {
  cur_state = TEXT_MODE;
  lores = false;
  fbuf_addr = 0;
  cancel_repeating_timer(&timer);
  fill_rect(0, 0, 640, 480, CYAN);
  set_text_size(1);
  set_text_wrap(1);
  set_cursor(0, 0);
  set_text_color(BLACK, CYAN);
  add_repeating_timer_ms(500, toggle_cursor, NULL, &timer);
}

void text_task(uint8_t byte);
void bitmap_task(uint8_t byte);

void dbus_task(void) {
  uint8_t byte;
  if (get_dbus_byte(&byte)) {
    if (byte == RESET) {
      vga_reset();
      return;
    }
    switch (cur_state) {
    case TEXT_MODE:
      text_task(byte);
      break;
    case BITMAP_MODE:
      bitmap_task(byte);
      break;
    default:
      break;
    }
  }
}

void text_task(uint8_t byte) {
  switch (byte) {
  case GO_BITMAP:
    cancel_repeating_timer(&timer);
    fill_rect(cursor_x, cursor_y, 6, 8, textbgcolor);
    cur_state = BITMAP_MODE;
    break;
  case CLEAR:
    fill_rect(0, 0, 640, 480, textbgcolor);
    cursor_x = 0;
    cursor_y = 0;
    break;
  case BACKSPACE:
    fill_rect(cursor_x, cursor_y, 6, 8, textbgcolor);
    cursor_x -= textsize * 6;
    if (cursor_x < 0) {
      cursor_y -= textsize * 8;
      cursor_x = (WIDTH - (textsize * 6));
    }
    fill_rect(cursor_x, cursor_y, 6 * textsize, 8 * textsize, textbgcolor);
    break;
  default:
    cancel_repeating_timer(&timer);
    fill_rect(cursor_x, cursor_y, 6, 8, textbgcolor);
    draw_char_cursor(byte);
    add_repeating_timer_ms(500, toggle_cursor, NULL, &timer);
    break;
  }
}

void bitmap_task(uint8_t byte) {
  switch (byte) {
  case DRAW_PIXEL: //draw pixel xlo xhi ylo yhi color
    {
      uint16_t x = (uint16_t)get_dbus_byte_blocking();
      x = x | ((uint16_t)get_dbus_byte_blocking() << 8);
      uint16_t y = (uint16_t)get_dbus_byte_blocking();
      y = y | ((uint16_t)get_dbus_byte_blocking() << 8);
      uint8_t color = get_dbus_byte_blocking();
      draw_pixel(x, y, color);
    }
    break;
  case GO_LORES:
    lores = true;
    break;
  case GO_HIRES:
    lores = false;
    break;
  case DRAW_PIX_SEQ: //pixel data in low nibble of command
  case DRAW_PIX_SEQ+1:
  case DRAW_PIX_SEQ+2:
  case DRAW_PIX_SEQ+3:
  case DRAW_PIX_SEQ+4:
  case DRAW_PIX_SEQ+5:
  case DRAW_PIX_SEQ+6:
  case DRAW_PIX_SEQ+7:
  case DRAW_PIX_SEQ+8:
  case DRAW_PIX_SEQ+9:
  case DRAW_PIX_SEQ+10:
  case DRAW_PIX_SEQ+11:
  case DRAW_PIX_SEQ+12:
  case DRAW_PIX_SEQ+13:
  case DRAW_PIX_SEQ+14:
  case DRAW_PIX_SEQ+15:
    {
      uint8_t color = byte & 0x0f;
      if (!lores) {
	uint16_t y = (fbuf_addr / WIDTH) + 1;
	uint16_t x = (fbuf_addr % WIDTH) + 1;
	draw_pixel(x, y, color);
	fbuf_addr++;
	if (fbuf_addr >= (WIDTH * HEIGHT))
	  fbuf_addr = 0;
      } else {
	uint16_t y = (fbuf_addr / 128) * 5;
	uint16_t x = (fbuf_addr % 128) * 5;
	fill_rect(x, y, 5, 5, color);
	fbuf_addr++;
	if (fbuf_addr >= (128 * 96))
	  fbuf_addr = 0;
      }
    }
    break;
  case WRITE_RAW: //byte
    {
      uint8_t byte = get_dbus_byte_blocking();
      framebuf[fbuf_addr] = byte;
    }
    break;
  case DRAW_TRI: //draw_tri x1[2] y1[2] x2[2] y2[2] x3[2] y3[2] color
    {
      uint16_t x1 = (uint16_t)get_dbus_byte_blocking();
      x1 = x1 | ((uint16_t)get_dbus_byte_blocking() << 8);
      uint16_t y1 = (uint16_t)get_dbus_byte_blocking();
      y1 = y1 | ((uint16_t)get_dbus_byte_blocking() << 8);
      
      uint16_t x2 = (uint16_t)get_dbus_byte_blocking();
      x2 = x2 | ((uint16_t)get_dbus_byte_blocking() << 8);
      uint16_t y2 = (uint16_t)get_dbus_byte_blocking();
      y2 = y2 | ((uint16_t)get_dbus_byte_blocking() << 8);
      
      uint16_t x3 = (uint16_t)get_dbus_byte_blocking();
      x3 = x3 | ((uint16_t)get_dbus_byte_blocking() << 8);
      uint16_t y3 = (uint16_t)get_dbus_byte_blocking();
      y3 = y3 | ((uint16_t)get_dbus_byte_blocking() << 8);
      uint8_t color = get_dbus_byte_blocking();
      
      draw_tri(x1, y1, x2, y2, x3, y3, color);
    }
    break;
  case CLEAR: //clear color
    {
      uint8_t color = get_dbus_byte_blocking();
      fill_rect(0, 0, 640, 480, color);
    }
    break;
  case GO_TEXT:
    add_repeating_timer_ms(500, toggle_cursor, NULL, &timer);
    cur_state = TEXT_MODE;
    break;
  default:
    break;
  }
}

void tile_task(uint8_t byte) {

}

typedef struct {
  float x;
  float y;
  float z;
} vec3;

typedef struct {
  unsigned short rows;
  unsigned short cols;
  float m[4][4];
} matrix;

matrix newmat(unsigned short r, unsigned short c) {
  matrix m = { r, c, {
	   {0, 0, 0, 0},
	   {0, 0, 0, 0},
	   {0, 0, 0, 0},
	   {0, 0, 0, 0}}};
  return m;
}

matrix v2m(vec3 v) {
  matrix result = newmat(4, 1);
  result.m[0][0] = v.x;
  result.m[1][0] = v.y;
  result.m[2][0] = v.z;
  result.m[3][0] = 1.;
  return result;
}

vec3 m2v(matrix m) {
  vec3 result = { m.m[0][0] / m.m[3][0],
		  m.m[1][0] / m.m[3][0],
		  m.m[2][0] / m.m[3][0]};
  return result;
}

matrix matmul(matrix a, matrix b) {
  matrix result = newmat(a.rows, b.cols);
  for (int i = 0; i < a.rows; i++) {
    for (int j = 0; j < b.cols; j++) {
      result.m[i][j] = 0.;
      for (int k = 0; k < a.cols; k++) {
	result.m[i][j] += a.m[i][k] * b.m[k][j];
      }
    }
  }
  return result;
}

typedef struct {
  vec3 v1;
  vec3 v2;
  vec3 v3;
} tri;

void identity(matrix *m) {
  for (int i = 0; i < m->cols; i++) {
    for (int j = 0; j < m->rows; j++) {
      m->m[i][j] = i == j ? 1. : 0.;
    }
  }
}

vec3 project(vec3 v) {
  vec3 result = { (v.x + 1.) * WIDTH/2., (v.y + 1.) * HEIGHT/2., (v.z + 1.) * 255./2. };
  return result;
}

vec3 persp(vec3 v) {
  float c = 1. - v.z / 3.;
  vec3 result;
  result.x = v.x / c;
  result.y = v.y / c;
  result.z = v.z / c;
  return result;
}

vec3 rot(vec3 v) {
  float a = M_PI/6.;
  float cosy = cosf(a);
  float siny = sinf(a);
  matrix roty = newmat(3, 3);
  roty.m[0][0] = cosy;
  roty.m[0][1] = 0.;
  roty.m[0][2] = siny;

  roty.m[1][0] = 0.;
  roty.m[1][1] = 1.;
  roty.m[1][2] = 0.;

  roty.m[2][0] = -siny;
  roty.m[2][1] = 0.;
  roty.m[2][2] = cosy;
  return m2v(matmul(roty, v2m(v)));
}

void threed_demo() {
  tri cube_faces[12] = {
    { {0, 0, -0.8}, {0, 0, 0}, {0, -0.8, 0} },
    { {0, 0, -0.8}, {0, 0, 0}, {0, -0.8, -0.8} },

    { {-0.8, -0.8, -0.8}, {-0.8, -0.8, 0}, {-0.8, 0, 0} },
    { {-0.8, -0.8, -0.8}, {-0.8, 0, 0}, {-0.8, 0, -0.8} },

    { {0, 0, 0}, {0, 0, -0.8}, {-0.8, 0, -0.8} },
    { {0, 0, 0}, {-0.8, 0, -0.8}, {-0.8, 0, 0} },

    { {0, -0.8, -0.8}, {0, -0.8, 0}, {-0.8, -0.8, 0} },
    { {0, -0.8, -0.8}, {-0.8, -0.8, 0}, {-0.8, -0.8, -0.8} },

    { {0, -0.8, 0}, {0, 0, 0}, {-0.8, 0, 0} },
    { {0, -0.8, 0}, {-0.8, 0, 0}, {-0.8, -0.8, 0} },

    { {0, 0, -0.8}, {0, -0.8, -0.8}, {-0.8, -0.8, -0.8} },
    { {0, 0, -0.8}, {-0.8, -0.8, -0.8}, {-0.8, 0, -0.8} }
  };

  matrix viewport = newmat(4, 4);
  identity(&viewport);
  viewport.m[0][3] = WIDTH/2.;
  viewport.m[1][3] = HEIGHT/2.;
  viewport.m[2][3] = 255/2.;
  viewport.m[0][0] = WIDTH/2.;
  viewport.m[1][1] = HEIGHT/2.;
  viewport.m[2][2] = 255/2.;

  matrix projection = newmat(4, 4);
  identity(&projection);
  projection.m[3][2] = -1./-2.;

  float pi180 = 3.14159/180.f;
  float cosz = cosf(15. * pi180);
  float sinz = sinf(15. * pi180);
  float cosy = cosf(5. * pi180);
  float siny = sinf(5. * pi180);

  matrix roty = newmat(3, 3);
  roty.m[0][0] = cosy;
  roty.m[0][1] = 0.;
  roty.m[0][2] = siny;

  roty.m[1][0] = 0.;
  roty.m[1][1] = 1.;
  roty.m[1][2] = 0.;

  roty.m[2][0] = -siny;
  roty.m[2][1] = 0.;
  roty.m[2][2] = cosy;

  matrix rotz = newmat(3, 3);
  rotz.m[0][0] = cosz;
  rotz.m[0][1] = -sinz;
  rotz.m[0][2] = 0.;

  rotz.m[1][0] = sinz;
  rotz.m[1][1] = cosz;
  rotz.m[1][2] = 0.;

  rotz.m[2][0] = 0.;
  rotz.m[2][1] = 0.;
  rotz.m[2][2] = 1.;

  //matrix rot = matmul(rotz, roty);

  for (int i = 0; i < 12; i++) {
    //vec3 screenv1 =
    draw_tri((short)(project(persp(rot(cube_faces[i].v1))).x),
	     (short)(project(persp(rot(cube_faces[i].v1))).y),

	     (short)(project(persp(rot(cube_faces[i].v2))).x),
	     (short)(project(persp(rot(cube_faces[i].v2))).y),

	     (short)(project(persp(rot(cube_faces[i].v3))).x),
	     (short)(project(persp(rot(cube_faces[i].v3))).y),
	     RED);
  }
}

void main() {
  set_sys_clock_khz(150000, true);
  stdio_init_all();

  dbus_rptr = dbus_wptr = dbus_buf;
  dbus_pio = pio0;
  dbus_pio_irq = PIO0_IRQ_0;
  dbus_offset = pio_add_program(dbus_pio, &dbus_program);
  dbus_sm = pio_claim_unused_sm(dbus_pio, true);
  dbus_program_init(dbus_pio, dbus_sm, dbus_offset, D0);

  pio_set_irq0_source_enabled(dbus_pio, pis_interrupt0, true);
  irq_set_exclusive_handler(dbus_pio_irq, read_dbus);
  irq_set_enabled(dbus_pio_irq, true);
  pio_sm_set_enabled(dbus_pio, dbus_sm, true);

  init_VGA();
  fill_rect(0, 0, 640, 480, CYAN);
  set_text_size(1);
  set_text_wrap(1);
  set_cursor(0, 0);
  set_text_color(BLACK, CYAN);

  add_repeating_timer_ms(500, toggle_cursor, NULL, &timer);

  //threed_demo();

  while (true) {
    dbus_task();
  }
}
