#include <stdint.h>

#define MEM_READB(x) (*((volatile uint8_t *)x))
#define MEM_WRITEB(x, y) ((*((volatile uint8_t *)x)) = y)
#define MEM_READW(x) (*(volatile uint16_t *)x)
#define MEM_WRITEW(x, y) ((*((volatile uint16_t *)x)) = y)

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

#define CF_BASE 0x180000
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

void hexdump(uint8_t *buf, int count) {
  if (count == 0)
    return;
  for (int i = 0; i < count; i += 16) {
    for (int j = 0; j < 16; j++) {
      if ((i + j) >= count)
	break;
      prbyte(buf[i+j]);
      putch(' ');
    }
    putch('\r');
    putch('\n');
  }
}

void ata_drq_wait() {
  while (!(*((volatile uint8_t *)CF_STAT) & CF_STAT_DRQ))
    asm volatile ("");
}

void ata_busy_wait() {
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

uint32_t le2be(uint8_t *le) {
  uint32_t be = 0;
  be |= (uint32_t)le[0];
  be |= (uint32_t)le[1] << 8;
  be |= (uint32_t)le[2] << 16;
  be |= (uint32_t)le[3] << 24;
  return be;
}

uint16_t le2be16(uint8_t *le) {
  uint16_t be = 0;
  be |= (uint16_t)le[0];
  be |= (uint16_t)le[1] << 8;
  return be;
}

struct {
  uint16_t type_perm;
  uint16_t uid;
  uint64_t size;
  uint32_t atime;
  uint32_t ctime;
  uint32_t mtime;
  uint32_t dtime;
  uint16_t gid;
  uint16_t nlinks;
  uint32_t nsectors;
  uint32_t flags;
  uint32_t os_res_1;
  uint32_t dbptr0;
  uint32_t dbptr1;
  uint32_t dbptr2;
  uint32_t dbptr3;
  uint32_t dbptr4;
  uint32_t dbptr5;
  uint32_t dbptr6;
  uint32_t dbptr7;
  uint32_t dbptr8;
  uint32_t dbptr9;
  uint32_t dbptr10;
  uint32_t dbptr11;
  uint32_t sibptr;
  uint32_t dibptr;
  uint32_t tibptr;
  uint32_t ngen;
  uint32_t file_acl;
  uint32_t dir_acl;
  uint32_t frag_blk;
  uint8_t os_res_2[12];
} ino;

//replace with version that takes ino struct and loads off disk as needed
void print_dir(uint8_t *buf) {
  uint32_t ino_num = le2be(&(buf[0]));
  uint16_t size = le2be16(&(buf[4]));
  uint8_t name_len = buf[6];
  uint8_t type = buf[7];

  int j = 0;
  while (j < 512) {
    if (ino_num == 0)
      break;
    switch (type) {
    case 0:
      putstr("U ");
      break;
    case 1:
      putstr("F ");
      break;
    case 2:
      putstr("D ");
      break;
    default:
      putstr("X ");
      break;
    }
    for (int i = 0; i < name_len; i++) {
      putch(buf[8+i+j]);
    }
    putstr("\r\n");
    j += size;
    ino_num = le2be(&(buf[j+0]));
    size = le2be16(&(buf[j+4]));
    name_len = buf[j+6];
    type = buf[j+7];
  }
}

//rework to take dir's inode
uint32_t find_name(uint8_t *buf, char *name) {
  uint32_t ino_num = le2be(&(buf[0]));
  uint16_t size = le2be16(&(buf[4]));
  uint8_t name_len = buf[6];
  uint8_t type = buf[7];

  int j = 0;
  while (j < 512) {
    if (ino_num == 0)
      break;
    int i = 0;
    for (; i < name_len; i++) {
      if ((buf[8+i+j] != name[i]) || (name[i] == '\0'))
	break;
    }
    if (i == name_len)
      return ino_num;
    j += size;
    ino_num = le2be(&(buf[j+0]));
    size = le2be16(&(buf[j+4]));
    name_len = buf[j+6];
    type = buf[j+7];
  }
  return 0;
}
    
uint8_t gptbuf[512];
uint8_t superblockbuf[1024];

int main() {
  read_sector(2, gptbuf);
  //its actually 8 bytes but im lazy and using a small disk
  uint32_t part_start = 0;
  part_start |= (uint32_t)gptbuf[0x20];
  part_start |= (uint32_t)gptbuf[0x21] << 8;
  part_start |= (uint32_t)gptbuf[0x22] << 16;
  part_start |= (uint32_t)gptbuf[0x23] << 24;
  read_sector(part_start+2, superblockbuf);
  read_sector(part_start+3, superblockbuf+512);

  if (superblockbuf[58] == 1)
    putstr("File system clean.\r\n");
  else
    putstr("File system has errors.\r\n");

  uint32_t num_blocks = 0;
  num_blocks |= (uint32_t)superblockbuf[4];
  num_blocks |= (uint32_t)superblockbuf[5] << 8;
  num_blocks |= (uint32_t)superblockbuf[6] << 16;
  num_blocks |= (uint32_t)superblockbuf[7] << 24;
  putstr("Total blocks: ");
  prlong(num_blocks);
  putstr("\r\n");

  uint32_t num_inodes = 0;
  num_inodes |= (uint32_t)superblockbuf[0];
  num_inodes |= (uint32_t)superblockbuf[1] << 8;
  num_inodes |= (uint32_t)superblockbuf[2] << 16;
  num_inodes |= (uint32_t)superblockbuf[3] << 24;
  putstr("Total inodes: ");
  prlong(num_inodes);
  putstr("\r\n");

  uint32_t block_size = 0;
  block_size |= (uint32_t)superblockbuf[24];
  block_size |= (uint32_t)superblockbuf[25] << 8;
  block_size |= (uint32_t)superblockbuf[26] << 16;
  block_size |= (uint32_t)superblockbuf[27] << 24;
  block_size = 1024 << block_size;
  putstr("Block size: ");
  prlong(block_size);
  putstr("\r\n");

  uint32_t blocks_p_group = 0;
  blocks_p_group |= (uint32_t)superblockbuf[32];
  blocks_p_group |= (uint32_t)superblockbuf[33] << 8;
  blocks_p_group |= (uint32_t)superblockbuf[34] << 16;
  blocks_p_group |= (uint32_t)superblockbuf[35] << 24;
  putstr("Blocks per block group: ");
  prlong(blocks_p_group);
  putstr("\r\n");

  uint32_t inodes_p_group = 0;
  inodes_p_group |= (uint32_t)superblockbuf[40];
  inodes_p_group |= (uint32_t)superblockbuf[41] << 8;
  inodes_p_group |= (uint32_t)superblockbuf[42] << 16;
  inodes_p_group |= (uint32_t)superblockbuf[43] << 24;
  putstr("Inodes per block group: ");
  prlong(inodes_p_group);
  putstr("\r\n");

  uint32_t total_groups = num_blocks / blocks_p_group;
  if (total_groups != num_inodes / inodes_p_group)
    putstr("Inconsistent metadata or bad math\r\n");
  putstr("Total block groups: ");
  prlong(total_groups);
  putstr("\r\n");

  uint32_t vers_maj = le2be(&(superblockbuf[76]));
  uint32_t vers_min = 0;
  vers_min |= (uint32_t)superblockbuf[62];
  vers_min |= (uint32_t)superblockbuf[63] << 8;
  putstr("Version: ");
  prlong(vers_maj);
  putch('.');
  prlong(vers_min);
  putstr("\r\n");
  
  if (vers_maj >= 1) {
    putstr("Extended fields:\r\n");
    uint32_t feat_opt = le2be(&(superblockbuf[92]));
    uint32_t feat_req = le2be(&(superblockbuf[96]));
    uint32_t feat_ro = le2be(&(superblockbuf[100]));
    putstr("Features: (Req.-Opt.-R/O) ");
    prlong(feat_req);
    putch('-');
    prlong(feat_opt);
    putch('-');
    prlong(feat_ro);
    putstr("\r\n");

    putstr("Volume name: ");
    putstr(&(superblockbuf[120]));
    putstr("\r\n");
  }

  uint32_t superblock_blknum = le2be(&(superblockbuf[20]));
  uint32_t bgdt_blknum = superblock_blknum+1;
  uint32_t bgdt_lba = part_start + (bgdt_blknum * (block_size / 512));

  putstr("BGDT start block: ");
  prlong(bgdt_blknum);
  putstr("\r\n");
  putstr("BGDT LBA: ");
  prlong(bgdt_lba);
  putstr("\r\n");

  uint8_t bgdt_buf[512];
  read_sector(bgdt_lba, bgdt_buf);

  uint32_t root_ino = 2;
  putstr("Root dir inode is in block group ");
  prlong((root_ino - 1) / inodes_p_group);
  putstr("\r\n");
  //going to cheat since i already know its in 0 and already have that gdt
  uint32_t ino_table_blk = le2be(&(bgdt_buf[8]));
  uint8_t ino_tbl_buf[512];
  read_sector(part_start + (ino_table_blk * (block_size / 512)), ino_tbl_buf);
  uint32_t ino_index = (root_ino - 1) % inodes_p_group;
  uint32_t ino_size = le2be(&(superblockbuf[88])); //128 in v<1
  putstr("Inode size: ");
  prlong(ino_size);
  putstr("\r\n");
  uint32_t ino_blk = (ino_index * ino_size) / block_size;
  putstr("Inode index: ");
  prlong(ino_index);
  putstr("\r\n");
  putstr("Inode's block: ");
  prlong(ino_blk);
  putstr("\r\n");

  uint8_t ino_buf[256];
  for (int i = 0; i < 256; i++) {
    ino_buf[i] = ino_tbl_buf[256+i];
  }

  uint32_t dbptr0 = le2be(&(ino_buf[40]));
  read_sector(part_start + ((dbptr0 * block_size) / 512), gptbuf);
  print_dir(gptbuf);
  uint32_t txt_ino = find_name(gptbuf, "a.txt");
  if (txt_ino == 0) {
    putstr("Couldn't find a.txt\r\n");
  } else {
    putstr("a.txt inode number: ");
    prlong(txt_ino);
    putstr("\r\n");
  }
  uint32_t txt_index = (txt_ino - 1) % inodes_p_group;
  uint32_t ino_lba_offs = (txt_index * ino_size) / 512;
  read_sector(part_start + ((ino_table_blk * block_size) / 512) + ino_lba_offs, gptbuf);
  for (int i = 0; i < 256; i++) {
    ino_buf[i] = gptbuf[256+i];
  }
  dbptr0 = le2be(&(ino_buf[40]));
  putstr("a.txt size: ");
  prlong(le2be(&(ino_buf[4])));
  putstr("\r\n");
  putstr("a.txt contents:\r\n");
  read_sector(part_start + ((dbptr0 * block_size) / 512), gptbuf);
  for (int i = 0; gptbuf[i] != '\0'; i++)
    putch(gptbuf[i]);
  putstr("\r\n");
  return 0;
}
    
