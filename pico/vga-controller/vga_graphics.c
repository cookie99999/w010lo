/*
 * https://vanhunteradams.com/Pico/VGA/VGA.html
 */
#include <stdio.h>
#include <stdlib.h>
#include <stdint.h>
#include <string.h>
#include "pico/stdlib.h"
#include "hardware/pio.h"
#include "hardware/dma.h"
// Our assembled programs:
// Each gets the name <pio_filename.pio.h>
#include "hsync.pio.h"
#include "vsync.pio.h"
#include "rgb.pio.h"
// Header file
#include "vga_graphics.h"
// Font file
#include "glcdfont.c"

// VGA timing constants
#define H_ACTIVE   655    // (active + frontporch - 1) - one cycle delay for mov
#define V_ACTIVE   479    // (active - 1)
#define RGB_ACTIVE 319    // (horizontal active)/2 - 1
// #define RGB_ACTIVE 639 // change to this if 1 pixel/byte

// Length of the pixel array, and number of DMA transfers
#define TXCOUNT 153600 // Total pixels/2 (since we have 2 pixels per byte)

// Pixel color array that is DMA's to the PIO machines and
// a pointer to the ADDRESS of this color array.
// Note that this array is automatically initialized to all 0's (black)
unsigned char framebuf[TXCOUNT];
char *address_pointer = &framebuf[0] ;

// For drawLine
#define swap(a, b) { short t = a; a = b; b = t; }

// For writing text
#define tabspace 4 // number of spaces for a tab

// For accessing the font library
#define pgm_read_byte(addr) (*(const uint8_t*)(addr))

// For drawing characters
unsigned short cursor_y, cursor_x, textsize ;
char textcolor, textbgcolor, wrap;

// Screen width/height
#define _width 640
#define _height 480

void init_VGA() {
  // Choose which PIO instance to use (there are two instances, each with 4 state machines)
  PIO pio = pio1;

  // Our assembled program needs to be loaded into this PIO's instruction
  // memory. This SDK function will find a location (offset) in the
  // instruction memory where there is enough space for our program. We need
  // to remember these locations!
  //
  // We only have 32 instructions to spend! If the PIO programs contain more than
  // 32 instructions, then an error message will get thrown at these lines of code.
  //
  // The program name comes from the .program part of the pio file
  // and is of the form <program name_program>
  uint hsync_offset = pio_add_program(pio, &hsync_program);
  uint vsync_offset = pio_add_program(pio, &vsync_program);
  uint rgb_offset = pio_add_program(pio, &rgb_program);

  // Manually select a few state machines from pio instance pio0.
  uint hsync_sm = 0;
  uint vsync_sm = 1;
  uint rgb_sm = 2;
  pio_sm_claim(pio, hsync_sm);
  pio_sm_claim(pio, vsync_sm);
  pio_sm_claim(pio, rgb_sm);

  // Call the initialization functions that are defined within each PIO file.
  // Why not create these programs here? By putting the initialization function in
  // the pio file, then all information about how to use/setup that state machine
  // is consolidated in one place. Here in the C, we then just import and use it.
  hsync_program_init(pio, hsync_sm, hsync_offset, HSYNC);
  vsync_program_init(pio, vsync_sm, vsync_offset, VSYNC);
  rgb_program_init(pio, rgb_sm, rgb_offset, LO_GRN);


  /////////////////////////////////////////////////////////////////////////////////////////////////////
  // ============================== PIO DMA Channels =================================================
  /////////////////////////////////////////////////////////////////////////////////////////////////////

  // DMA channels - 0 sends color data, 1 reconfigures and restarts 0
  int rgb_chan_0 = dma_claim_unused_channel(true);
  int rgb_chan_1 = dma_claim_unused_channel(true);

  // Channel Zero (sends color data to PIO VGA machine)
  dma_channel_config c0 = dma_channel_get_default_config(rgb_chan_0);  // default configs
  channel_config_set_transfer_data_size(&c0, DMA_SIZE_8);              // 8-bit txfers
  channel_config_set_read_increment(&c0, true);                        // yes read incrementing
  channel_config_set_write_increment(&c0, false);                      // no write incrementing
  channel_config_set_dreq(&c0, DREQ_PIO1_TX2) ;                        // DREQ_PIO1_TX2 pacing (FIFO)
  channel_config_set_chain_to(&c0, rgb_chan_1);                        // chain to other channel

  dma_channel_configure(
			rgb_chan_0,                 // Channel to be configured
			&c0,                        // The configuration we just created
			&pio->txf[rgb_sm],          // write address (RGB PIO TX FIFO)
			&framebuf,            // The initial read address (pixel color array)
			TXCOUNT,                    // Number of transfers; in this case each is 1 byte.
			false                       // Don't start immediately.
			);

  // Channel One (reconfigures the first channel)
  dma_channel_config c1 = dma_channel_get_default_config(rgb_chan_1);   // default configs
  channel_config_set_transfer_data_size(&c1, DMA_SIZE_32);              // 32-bit txfers
  channel_config_set_read_increment(&c1, false);                        // no read incrementing
  channel_config_set_write_increment(&c1, false);                       // no write incrementing
  channel_config_set_chain_to(&c1, rgb_chan_0);                         // chain to other channel

  dma_channel_configure(
			rgb_chan_1,                         // Channel to be configured
			&c1,                                // The configuration we just created
			&dma_hw->ch[rgb_chan_0].read_addr,  // Write address (channel 0 read address)
			&address_pointer,                   // Read address (POINTER TO AN ADDRESS)
			1,                                  // Number of transfers, in this case each is 4 byte
			false                               // Don't start immediately.
			);

  /////////////////////////////////////////////////////////////////////////////////////////////////////
  /////////////////////////////////////////////////////////////////////////////////////////////////////

  // Initialize PIO state machine counters. This passes the information to the state machines
  // that they retrieve in the first 'pull' instructions, before the .wrap_target directive
  // in the assembly. Each uses these values to initialize some counting registers.
  pio_sm_put_blocking(pio, hsync_sm, H_ACTIVE);
  pio_sm_put_blocking(pio, vsync_sm, V_ACTIVE);
  pio_sm_put_blocking(pio, rgb_sm, RGB_ACTIVE);


  // Start the two pio machine IN SYNC
  // Note that the RGB state machine is running at full speed,
  // so synchronization doesn't matter for that one. But, we'll
  // start them all simultaneously anyway.
  pio_enable_sm_mask_in_sync(pio, ((1u << hsync_sm) | (1u << vsync_sm) | (1u << rgb_sm)));

  // Start DMA channel 0. Once started, the contents of the pixel color array
  // will be continously DMA's to the PIO machines that are driving the screen.
  // To change the contents of the screen, we need only change the contents
  // of that array.
  dma_start_channel_mask((1u << rgb_chan_0)) ;
}


// A function for drawing a pixel with a specified color.
// Note that because information is passed to the PIO state machines through
// a DMA channel, we only need to modify the contents of the array and the
// pixels will be automatically updated on the screen.
void draw_pixel(short x, short y, uint8_t color) {
  // Range check (640x480 display)
  if ((x >= _width) | (x < 0) | (y >= _height) | (y < 0))
    return;

  // Which pixel is it?
  int pixel = ((_width * y) + x);

  // Is this pixel stored in the first 3 bits
  // of the vga data array index, or the second
  // 3 bits? Check, then mask.
  if (pixel & 1) {
    framebuf[pixel>>1] = (framebuf[pixel>>1] & 0x0f) | (color << 4);
  } else {
    framebuf[pixel>>1] = (framebuf[pixel>>1] & 0xf0) | color;
  }
}

void draw_v_line(short x, short y, short h, uint8_t color) {
  for (short i = y; i < (y + h); i++) {
    draw_pixel(x, i, color) ;
  }
}

void draw_h_line(short x, short y, short w, uint8_t color) {
  // range checks
  if ((x >= _width) || (y >= _height))
    return;
  if ((x + w - 1) >= _width)
    w = _width - x - 1;
  
  short both_color = color | (color << 4) ;
  // loner pixel at x -- align left with next byte boundary
  if (x & 1) {
    draw_pixel(x, y, color);
    x++;
    w--;
  }
  // draw loner pixel at end and adjust width
  if (w & 1) {
    draw_pixel(x + w - 1, y, color);
    w--;
  }
  // draw rest of line
  int len = (w >> 1)  ;
  if (len > 0 && y < _height)
    memset(&framebuf[(_width / 2) * y + (x >> 1)], both_color, len) ;
}

// replaced with rosetta code copypaste
void draw_line(short x0, short y0, short x1, short y1, uint8_t color) {
  int dx = abs(x1-x0), sx = x0<x1 ? 1 : -1;
  int dy = abs(y1-y0), sy = y0<y1 ? 1 : -1; 
  int err = (dx>dy ? dx : -dy)/2, e2;

  for(;;){
    draw_pixel(x0, y0, color);
    if (x0==x1 && y0==y1) break;
    e2 = err;
    if (e2 >-dx) { err -= dy; x0 += sx; }
    if (e2 < dy) { err += dx; y0 += sy; }
  }
}

void draw_tri(short x1, short y1, short x2, short y2, short x3, short y3, uint8_t color) {
  draw_line(x1, y1, x2, y2, color);
  draw_line(x2, y2, x3, y3, color);
  draw_line(x3, y3, x1, y1, color);
}

// Draw a rectangle
void draw_rect(short x, short y, short w, short h, uint8_t color) {
  /* Draw a rectangle outline with top left vertex (x,y), width w
   * and height h at given color
   * Parameters:
   *      x:  x-coordinate of top-left vertex. The x-coordinate of
   *          the top-left of the screen is 0. It increases to the right.
   *      y:  y-coordinate of top-left vertex. The y-coordinate of
   *          the top-left of the screen is 0. It increases to the bottom.
   *      w:  width of the rectangle
   *      h:  height of the rectangle
   *      color:  16-bit color of the rectangle outline
   * Returns: Nothing
   */
  draw_h_line(x, y, w, color);
  draw_h_line(x, y+h-1, w, color);
  draw_v_line(x, y, h, color);
  draw_v_line(x+w-1, y, h, color);
}

void drawCircle(short x0, short y0, short r, char color) {
  /* Draw a circle outline with center (x0,y0) and radius r, with given color
   * Parameters:
   *      x0: x-coordinate of center of circle. The top-left of the screen
   *          has x-coordinate 0 and increases to the right
   *      y0: y-coordinate of center of circle. The top-left of the screen
   *          has y-coordinate 0 and increases to the bottom
   *      r:  radius of circle
   *      color: 16-bit color value for the circle. Note that the circle
   *          isn't filled. So, this is the color of the outline of the circle
   * Returns: Nothing
   */
  short f = 1 - r;
  short ddF_x = 1;
  short ddF_y = -2 * r;
  short x = 0;
  short y = r;

  draw_pixel(x0, y0 + r, color);
  draw_pixel(x0, y0 - r, color);
  draw_pixel(x0 + r, y0, color);
  draw_pixel(x0 - r, y0, color);

  while (x < y) {
    if (f >= 0) {
      y--;
      ddF_y += 2;
      f += ddF_y;
    }
    x++;
    ddF_x += 2;
    f += ddF_x;

    draw_pixel(x0 + x, y0 + y, color);
    draw_pixel(x0 - x, y0 + y, color);
    draw_pixel(x0 + x, y0 - y, color);
    draw_pixel(x0 - x, y0 - y, color);
    draw_pixel(x0 + y, y0 + x, color);
    draw_pixel(x0 - y, y0 + x, color);
    draw_pixel(x0 + y, y0 - x, color);
    draw_pixel(x0 - y, y0 - x, color);
  }
}

// fill a rectangle
void fill_rect(short x, short y, short w, short h, uint8_t color) {
  /* Draw a filled rectangle with starting top-left vertex (x,y),
   *  width w and height h with given color
   * Parameters:
   *      x:  x-coordinate of top-left vertex; top left of screen is x=0
   *              and x increases to the right
   *      y:  y-coordinate of top-left vertex; top left of screen is y=0
   *              and y increases to the bottom
   *      w:  width of rectangle
   *      h:  height of rectangle
   *      color:  3-bit color value
   * Returns:     Nothing
   */
  if ((y + h - 1) >= _height)
    h = _height - y;
  for(int j = y; j < (y + h); j++) {
    draw_h_line(x, j, w, color) ;
  }
}

// Draw a character
void draw_char(short x, short y, unsigned char c, uint8_t color, uint8_t bg, unsigned char size) {
  if ((x >= _width) || // Clip right
     (y >= _height) || // Clip bottom
     ((x + 6 * size - 1) < 0) || // Clip left
     ((y + 8 * size - 1) < 0)) // Clip top
    return;

  for (char i = 0; i < 6; i++) {
    const uint8_t *line_offs = font + (c * 5) + i;
    //blank line at bottom of char, otherwise fetch from font
    uint8_t line = i == 5 ? 0 : pgm_read_byte(line_offs);
    
    for (char j = 0; j < 8; j++) {
      if (line & 1) {
        if (size == 1) // default size
          draw_pixel(x + i, y + j, color);
        else {  // big size
          fill_rect(x + (i * size), y + (j * size), size, size, color);
        }
      } else if (bg != color) {
        if (size == 1) // default size
          draw_pixel(x + i, y + j, bg);
        else {  // big size
          fill_rect(x + i * size, y + j * size, size, size, bg);
        }
      }
      line >>= 1;
    }
  }
}

inline void set_cursor(short x, short y) {
  /* Set cursor for text to be printed
   * Parameters:
   *      x = x-coordinate of top-left of text starting
   *      y = y-coordinate of top-left of text starting
   * Returns: Nothing
   */
  cursor_x = x;
  cursor_y = y;
}

inline void set_text_size(unsigned char s) {
  /*Set size of text to be displayed
   * Parameters:
   *      s = text size (1 being smallest)
   * Returns: nothing
   */
  textsize = (s > 0) ? s : 1;
}

inline void setTextColor(uint8_t c) {
  // For 'transparent' background, we'll set the bg
  // to the same as fg instead of using a flag
  textcolor = textbgcolor = c;
}

inline void set_text_color(uint8_t c, uint8_t b) {
  /* Set color of text to be displayed
   * Parameters:
   *      c = 16-bit color of text
   *      b = 16-bit color of text background
   */
  textcolor = c;
  textbgcolor = b;
}

inline void set_text_wrap(char w) {
  wrap = w;
}


void draw_char_cursor(uint8_t c){
  if (c == '\n') {
    cursor_y += textsize * 8;
    cursor_x = 0;
  } else if (c == '\r') {
    // skip em
  } else if (c == '\t') {
    int new_x = cursor_x + tabspace;
    if (new_x < _width) {
      cursor_x = new_x;
    }
  } else {
    draw_char(cursor_x, cursor_y, c, textcolor, textbgcolor, textsize);
    cursor_x += textsize * 6;
    if (wrap && (cursor_x > (_width - textsize * 6))) {
      cursor_y += textsize*8;
      cursor_x = 0;
    }
  }
  if (cursor_y >= _height) {
    memmove(framebuf, framebuf + ((_width / 2) * 8), TXCOUNT - ((_width / 2) * 8));
    fill_rect(0, _height - 8, _width, 8, textbgcolor);
    cursor_y -= textsize * 8;
  }
}

inline void write_string(char* str){
  /* Print text onto screen
   * Call tft_setCursor(), tft_setTextColor(), tft_setTextSize()
   *  as necessary before printing
   */
  while (*str){
    draw_char_cursor(*str++);
  }
}
