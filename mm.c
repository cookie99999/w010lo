/*
  This is basically just the Minix 1.0 memory manager
  with minor modification (doubly-linked). For use in an
  actual kernel I'd like to change it up some more to use
  boundary tags so you don't have to remember the size
  and pass it to kfree. Since much of it is transcribed
  from ast's book, here's the license notice:

  Copyright (c) 1987,1997, Prentice Hall
  All rights reserved.
  
  Redistribution and use of the MINIX operating system in source and
  binary forms, with or without modification, are permitted provided
  that the following conditions are met:
  
  * Redistributions of source code must retain the above copyright
  notice, this list of conditions and the following disclaimer.
  
  * Redistributions in binary form must reproduce the above
  copyright notice, this list of conditions and the following
  disclaimer in the documentation and/or other materials provided
  with the distribution.
  
  * Neither the name of Prentice Hall nor the names of the software
  authors or contributors may be used to endorse or promote
  products derived from this software without specific prior
  written permission.
  
  THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS, AUTHORS, AND
  CONTRIBUTORS ``AS IS'' AND ANY EXPRESS OR IMPLIED WARRANTIES,
  INCLUDING, BUT NOT LIMITED TO, THE IMPLIED WARRANTIES OF
  MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE ARE DISCLAIMED.
  IN NO EVENT SHALL PRENTICE HALL OR ANY AUTHORS OR CONTRIBUTORS BE
  LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR
  CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF
  SUBSTITUTE GOODS OR SERVICES; LOSS OF USE, DATA, OR PROFITS; OR
  BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY,
  WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT (INCLUDING NEGLIGENCE
  OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE OF THIS SOFTWARE,
  EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
*/

#include <stdint.h>
#include <stddef.h>

#define MEMSZ 0x100000
#define KSTACKSZ 0x10000
#define MEMTOP (MEMSZ - KSTACKSZ)
extern uint8_t __end;

#define NHOLE 128
struct hole {
  uint8_t *begin;
  uint32_t size;
  struct hole *next;
  struct hole *prev;
};
struct hole holes[NHOLE];
struct hole *hole_head;
struct hole *free_head;

void putch(char c) {
  asm volatile ("1: btst.b #2, 0xfa1013\n"
		"beq 1b\n"
		"move.b %0, 0xfa1017"
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

void prbyte(uint8_t b) {
  uint8_t hi = (b >> 4) & 0x0f;
  hi += 0x30;
  if (hi > '9')
    hi += 7;
  putch(hi);
  b = (b & 0x0f) + 0x30;
  if (b > '9')
    b += 7;
  putch(b);
}

void prlong(uint32_t l) {
  prbyte((uint8_t)(l >> 24));
  prbyte((uint8_t)(l >> 16));
  prbyte((uint8_t)(l >> 8));
  prbyte((uint8_t)(l & 0xff));
}

void init_mem() {
  for (struct hole *h = &holes[0]; h < &holes[NHOLE]; h++) {
    h->next = h + 1;
    h->prev = h - 1;
  }
  holes[0].next = NULL;
  holes[0].prev = NULL;
  holes[0].size = MEMTOP - (uint32_t)&__end;
  holes[0].begin = (uint8_t*)&__end;
  hole_head = &holes[0];
  free_head = &holes[1];
}

uint32_t max_hole() {
  uint32_t max = 0;
  struct hole *h = hole_head;
  while (h != NULL) {
    if (h->size > max)
      max = h->size;
  }
  return max;
}

void del_hole(struct hole *h) {
  if (h == hole_head)
    hole_head = h->next;
  else
    h->prev->next = h->next;

  h->next = free_head;
  free_head = h;
}

void merge_hole(struct hole *h) {
  struct hole *next = NULL;
  if ((next = h->next) != NULL) {
    if (h->begin + h->size == next->begin) {
      h->size += next->size;
      del_hole(next);
    }
  }

  struct hole *prev = NULL;
  if ((prev = h->prev) != NULL) {
    if (prev->begin + prev->size == h->begin) {
      prev->size += h->size;
      del_hole(h);
    }
  }
}

uint8_t *kmalloc(uint32_t size) {
  if (size & 1) {
    size++; //todo align better than this
  }
  uint8_t *old_base;
  struct hole *h = hole_head;
  while (h != NULL) {
    if (h->size >= size) {
      // suitable hole found
      old_base = h->begin;
      h->begin += size;
      h->size -= size;

      if (h->size != 0)
	return old_base;
      del_hole(h);
      return old_base;
    }
    h = h->next;
  }
  return NULL; // no hole found
}

void kfree(uint8_t *base, uint32_t size) {
  if (size & 1) {
    size++;
  }
  struct hole *h;
  struct hole *new;
  if ((new = free_head) == NULL) {
    putstr("PANIC: hole table full\r\n");
    return;
  }

  new->begin = base;
  new->size = size;
  free_head = new->next; // old free_head->next
  h = hole_head;

  if (h == NULL || base <= h->begin) { // lower addr, insert at top
    new->next = h;
    h->prev = new;
    hole_head = new;
    new->prev = NULL;
    merge_hole(new);
    return;
  }

  // find hole that should come right after new and insert before it
  while (h != NULL && base > h->begin) {
    h = h->next;
  }

  struct hole *before = h->prev;
  new->prev = before;
  new->next = h;
  before->next = new;
  h->prev = new;
  merge_hole(new);
}

void print_holes() {
  struct hole *h = hole_head;
  while (h != NULL) {
    putstr("Begin: ");
    prlong((uint32_t)h->begin);
    putstr(" End: ");
    prlong((uint32_t)h->begin + h->size);
    putstr("\r\n");
    h = h->next;
  }
}

int main() {
  putstr("Start of free memory: ");
  prlong((uint32_t)&__end);
  putstr("\r\n");
  init_mem();

  putstr("No allocs: \r\n");
  print_holes();
  char *dynstr = kmalloc(7);
  if (dynstr != NULL) {
    dynstr[0] = 'P';
    dynstr[1] = 'o';
    dynstr[2] = 'o';
    dynstr[3] = 'p';
    dynstr[4] = '\r';
    dynstr[5] = '\n';
    dynstr[6] = '\0';
    putstr(dynstr);
  }
  putstr("One alloc: \r\n");
  print_holes();
  uint8_t *dummy = kmalloc(65535);
  putstr("Two allocs: \r\n");
  print_holes();
  kfree(dynstr, 7);
  putstr("One free: \r\n");
  print_holes();
  kfree(dummy, 65535);
  putstr("Two frees: \r\n");
  print_holes();

  return 0;
}
