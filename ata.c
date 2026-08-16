/*
  functions for handling ata/compactflash basics
*/
#include <stdint.h>
#include <stddef.h>

#define CF_BASE 0xfa2000
//registers
#define CF_DATA CF_BASE+0
#define CF_ERR CF_BASE+3
#define CF_FEATURE CF_BASE+3
#define CF_SEC_COUNT CF_BASE+5
#define CF_SEC_NUM CF_BASE+7
#define CF_LBA_7_0 CF_BASE+7
#define CF_CYL_LOW CF_BASE+9
#define CF_LBA_15_8 CF_BASE+9
#define CF_CYL_HIGH CF_BASE+11
#define CF_LBA_23_16 CF_BASE+11
#define CF_HEAD CF_BASE+13
#define CF_LBA_27_24 CF_BASE+13
#define CF_STAT CF_BASE+15
#define CF_CMD CF_BASE+15

//status flags
#define CF_STAT_ERR 0x01
#define CF_STAT_COR 0x04
#define CF_STAT_DRQ 0x08
#define CF_STAT_DSC 0x10
#define CF_STAT_DWF 0x20
#define CF_STAT_RDY 0x40
#define CF_STAT_BSY 0x80

//ata commands
#define CF_IDENTIFY 0xec
#define CF_READ_SEC 0x20
#define CF_WRITE_SEC 0x30

#define MEM_READB(x) (*((volatile uint8_t *)x))
#define MEM_WRITEB(x, y) ((*((volatile uint8_t *)x)) = y)
#define MEM_READW(x) (*(volatile uint16_t *)x)
#define MEM_WRITEW(x, y) ((*((volatile uint16_t *)x)) = y)

static void ata_drq_wait() {
  while (!(*((volatile uint8_t *)CF_STAT) & CF_STAT_DRQ))
    asm volatile ("");
}

static void ata_busy_wait() {
  while (*((volatile uint8_t *)CF_STAT) & CF_STAT_BSY)
    asm volatile ("");
}

void ata_identify(uint8_t *buf) {
  MEM_WRITEB(CF_LBA_7_0, (uint8_t)0);
  MEM_WRITEB(CF_LBA_15_8, (uint8_t)0);
  MEM_WRITEB(CF_LBA_23_16, (uint8_t)0);
  MEM_WRITEB(CF_SEC_COUNT, (uint8_t)0);
  MEM_WRITEB(CF_CMD, (uint8_t)CF_IDENTIFY);

  ata_drq_wait();
  ata_busy_wait();
  
  asm volatile ("move.l %0, %%a0\n"
		"move.l %1, %%a1\n"
		"move.l #255,%%d0\n"
		"1:\n"
		"move.w (%%a0),(%%a1)+\n"
		"dbra %%d0,1b\n"
		:
		: "g" ((volatile uint16_t *)CF_DATA), "g" (buf)
		: "%a0", "%a1", "%d0");
}

void read_sector(uint32_t lba, uint8_t *buf) {
  MEM_WRITEB(CF_LBA_7_0, (uint8_t)lba);
  MEM_WRITEB(CF_LBA_15_8, (uint8_t)(lba >> 8));
  MEM_WRITEB(CF_LBA_23_16, (uint8_t)(lba >> 16));
  MEM_WRITEB(CF_LBA_27_24, (uint8_t)((lba >> 24) | 0xe0));
  MEM_WRITEB(CF_SEC_COUNT, (uint8_t)1);
  MEM_WRITEB(CF_CMD, (uint8_t)CF_READ_SEC);
  ata_drq_wait();
  ata_busy_wait();

  asm volatile ("move.l %0, %%a0\n"
		"move.l %1, %%a1\n"
		"move.l #255,%%d1\n"
		"1:\n"
		"move.w (%%a0),%%d0\n"
		"ror.w #8,%%d0\n"
		"move.w %%d0,(%%a1)+\n"
		"dbra %%d1,1b\n"
		:
		: "g" ((volatile uint16_t *)CF_DATA), "g" (buf)
		: "%a0", "%a1", "%d0", "%d1");
}
