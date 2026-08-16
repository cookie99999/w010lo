#include <stdint.h>
#include <stddef.h>
#include "ata.h"

#define INO_BGRP(x, y) ((x - 1) / y) // bgrp # of ino x w/ ino p grp y
#define INO_IDX(x, y) ((x - 1) % y) // idx in bgrp of ino x w/ ino p grp y
#define INO_BLK(x, y, z) ((x * y) / z) //blk of ino idx x w/ ino size y and blk size z
#define BLK_LBA(x, y, z) (y + ((x * z) / 512)) // lba of blknum x w/ partition start y and block size z

extern void *kmalloc(uint32_t size);
extern void kfree(void *base, uint32_t size);
extern void init_mem();
extern void *memset(void*, int, size_t);
void sector_dump(uint32_t);

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

void prword(uint16_t w) {
  prbyte((uint8_t)(w >> 8));
  prbyte((uint8_t)(w & 0xff));
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

struct ino {
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

  // not on disk
  uint32_t ino_num;
  uint16_t refcount;
  uint8_t dirty;
};

struct e2fs_bgd {
  uint32_t blk_bitmap; //blk # of blk usage bitmap
  uint32_t ino_bitmap;
  uint32_t ino_tbl;
  uint16_t n_unalloc_blks;
  uint16_t n_unalloc_inos;
  uint16_t n_dirs;
};

struct e2fs_bgd parse_bgd(uint8_t *buf) {
  struct e2fs_bgd b;
  b.blk_bitmap = le2be(&(buf[0]));
  b.ino_bitmap = le2be(&(buf[4]));
  b.ino_tbl = le2be(&(buf[8]));
  b.n_unalloc_blks = le2be16(&(buf[12]));
  b.n_unalloc_inos = le2be16(&(buf[14]));
  b.n_dirs = le2be16(&(buf[18]));

  return b;
}

struct e2fs_sblk {
  uint32_t n_inos; // total inodes in fs
  uint32_t n_blks; // total blocks in fs
  uint32_t n_res_blks; // blocks reserved for superuser
  uint32_t n_unalloc_blks; // unallocated blocks
  uint32_t n_unalloc_inos; // unallocated inodes
  uint32_t sblk_blk_num; // block containing the superblock
  uint32_t blk_sz; // 1024 << blk_sz = block size
  uint32_t frag_sz; // 1024 << frag_sz = fragment size
  uint32_t blks_p_bgrp; // blocks per block group
  uint32_t frags_p_bgrp; // fragments per block group
  uint32_t inos_p_bgrp; // inodes per block group
  uint32_t last_mount; // last mount time in posix time
  uint32_t last_write; // last write time in posix time
  uint16_t mounts_since_fsck;
  uint16_t max_mounts_before_fsck;
  uint16_t fs_state;
  uint16_t err_action;
  uint64_t version;
  uint32_t last_fsck_time;
  uint32_t fsck_interval;
  uint32_t creation_os_id; // os that created the volume
  uint16_t res_uid;
  uint16_t res_gid; // uid and gid that can use reserved blocks
  // extended fields
  uint32_t first_avail_ino; // first non reserved inode
  uint16_t ino_sz;
  uint16_t sblk_bgrp; // bgrp # of this superblock
  uint32_t opt_features;
  uint32_t req_features;
  uint32_t ro_features;
  uint8_t fs_id[16];
  char volume_name[16];
  uint8_t last_mount_path[64];
  uint32_t compression_algorithms;
  uint8_t prealloc_file_blks;
  uint8_t prealloc_dir_blks;
  // not using journal stuff
  uint32_t orphan_inos_head;
};

void copymem(uint8_t *src, uint8_t *dest, int n) {
  while (n--) {
    *dest++ = *src++;
  }
}

struct e2fs_sblk *parse_sblk(uint8_t *buf) {
  struct e2fs_sblk *s = kmalloc(sizeof(struct e2fs_sblk));
  if (s == NULL) {
    putstr("Couldn't allocate superblock struct\r\n");
    return NULL;
  }

  uint16_t sig = le2be16(&(buf[56]));
  if (sig != 0xef53) {
    putstr("Superblock does not contain valid signature\r\n");
    return NULL;
  }
  
  s->n_inos = le2be(&(buf[0]));
  s->n_blks = le2be(&(buf[4]));
  s->n_res_blks = le2be(&(buf[8]));
  s->n_unalloc_blks = le2be(&(buf[12]));
  s->n_unalloc_inos = le2be(&(buf[16]));
  s->sblk_blk_num = le2be(&(buf[20]));
  s->blk_sz = le2be(&(buf[24]));
  s->frag_sz = le2be(&(buf[28]));
  s->blks_p_bgrp = le2be(&(buf[32]));
  s->frags_p_bgrp = le2be(&(buf[36]));
  s->inos_p_bgrp = le2be(&(buf[40]));
  s->last_mount = le2be(&(buf[44]));
  s->last_write = le2be(&(buf[48]));
  s->mounts_since_fsck = le2be16(&(buf[52]));
  s->max_mounts_before_fsck = le2be16(&(buf[54]));
  s->fs_state = le2be16(&(buf[58]));
  s->err_action = le2be16(&(buf[60]));
  s->version = ((uint64_t)le2be(&(buf[76])) << 32) |
    (uint64_t)le2be16(&(buf[62]));
  s->last_fsck_time = le2be(&(buf[64]));
  s->fsck_interval = le2be(&(buf[68]));
  s->creation_os_id = le2be(&(buf[72]));
  s->res_uid = le2be16(&(buf[80]));
  s->res_gid = le2be16(&(buf[82]));

  if (s->version >= 0x0000000100000000) {
    s->first_avail_ino = le2be(&(buf[84]));
    s->ino_sz = le2be16(&(buf[88]));
    s->sblk_bgrp = le2be16(&(buf[90]));
    s->opt_features = le2be(&(buf[92]));
    s->req_features = le2be(&(buf[96]));
    s->ro_features = le2be(&(buf[100]));
    copymem(&(buf[104]), (uint8_t*)&s->fs_id, 16);
    copymem(&(buf[120]), (uint8_t*)&s->volume_name, 16);
    copymem(&(buf[136]), (uint8_t*)&s->last_mount_path, 64);
    s->compression_algorithms = le2be(&(buf[200]));
    s->prealloc_file_blks = buf[204];
    s->prealloc_dir_blks = buf[205];
    s->orphan_inos_head = le2be(&(buf[236]));
  }

  return s;
}

struct file_desc {
  uint16_t fd_id;
  struct ino *fd_ino;
  uint16_t fd_count;
  uint32_t fd_offs;
  uint8_t fd_dirty;
  uint16_t fd_mode;
};

struct e2fs_fsinfo {
  struct e2fs_sblk *sblk;
  struct e2fs_bgd *bgdt;
  uint32_t vol_start; // lba of volume
  uint64_t blk_sz; // actual sizes calculated from sblk
  uint64_t frag_sz;
  uint32_t n_bgrp; // total block groups
};

struct e2fs_fsinfo *init_fsinfo() {
  struct e2fs_fsinfo *fi = kmalloc(sizeof(struct e2fs_fsinfo));
  if (fi == NULL) {
    putstr("Could not allocate fsinfo struct\r\n");
    return NULL;
  }
  
  uint8_t *sec_buf = kmalloc(1024); // tmp buffer for sectors
  if (sec_buf == NULL) {
    putstr("Could not allocate sec_buf\r\n");
    return NULL;
  }
  /* comparing this many bytes pulls in memcpy for some reason
  read_sector(1, sec_buf); // check partition table header signature
  if (sec_buf[0] != 'E' || sec_buf[1] != 'F' || sec_buf[2] != 'I' ||
      sec_buf[3] != ' ' || sec_buf[4] != 'P' || sec_buf[5] != 'A' ||
      sec_buf[6] != 'R' || sec_buf[7] != 'T') {
    putstr("No valid GPT header found\r\n");
    return NULL;
  }
  */
  read_sector(2, sec_buf); // get partition entries
  //todo actually check the header to confirm what sector theyre in
  //its actually 8 bytes but im lazy and using a small disk
  fi->vol_start = le2be(&(sec_buf[0x20]));

  read_sector(fi->vol_start+2, sec_buf);
  read_sector(fi->vol_start+3, sec_buf+512); // get superblock
  fi->sblk = parse_sblk(sec_buf);
  if (fi->sblk == NULL) {
    putstr("Could not allocate superblock struct\r\n");
    return NULL;
  }

  fi->blk_sz = 1024 << fi->sblk->blk_sz;
  fi->frag_sz = 1024 << fi->sblk->frag_sz;
  uint32_t ino_res = fi->sblk->n_inos / fi->sblk->inos_p_bgrp;
  if ((fi->sblk->n_inos % fi->sblk->inos_p_bgrp) >
      fi->sblk->inos_p_bgrp / 2)
    ino_res++;
  uint32_t blk_res = fi->sblk->n_blks / fi->sblk->blks_p_bgrp;
  if ((fi->sblk->n_blks % fi->sblk->blks_p_bgrp) >
      fi->sblk->blks_p_bgrp / 2)
    blk_res++;
  if (ino_res != blk_res)
    putstr("Inconsistent header or bad math\r\n");
  fi->n_bgrp = ino_res;

  uint32_t bgdt_sz = fi->n_bgrp * 32;
  fi->bgdt = kmalloc(bgdt_sz);
  if (fi->bgdt == NULL) {
    putstr("Could not allocate bgdt\r\n");
    return NULL;
  }
  uint32_t n_whole_secs = bgdt_sz / 512;
  uint32_t remainder = bgdt_sz % 512;
  uint32_t bgdt_lba = fi->vol_start + ((fi->sblk->sblk_blk_num + 1) *
				       (fi->blk_sz / 512));

  for (int i = 0; i < n_whole_secs; i++) {
    read_sector(bgdt_lba + i, sec_buf);
    for (int j = 0; j < 16; j++) { // 16 bgds in each full sector
      fi->bgdt[j+(i*16)] = parse_bgd(sec_buf+(32*j));
    }
  }

  read_sector(bgdt_lba + n_whole_secs, sec_buf);
  for (int j = 0; j < (remainder/32); j++) {
    fi->bgdt[j+(n_whole_secs*16)] = parse_bgd(sec_buf+(32*j));
  }

  kfree(sec_buf, 1024);
  return fi;
}

#define N_FILE 20 // files per process
struct file_desc fdtab[N_FILE];
size_t fdtab_count = 0;

#define N_INO 30 // inodes per process
struct ino inotab[N_INO];

/*
  handle annoying byte swapping to make read_ino cleaner
*/
void byteswap_ino(struct ino *i, uint8_t *buf) { 
  i->type_perm = le2be16(&(buf[0]));
  i->uid = le2be16(&(buf[2]));
  i->size = (uint64_t)(le2be(&(buf[4]))) |
    ((uint64_t)(le2be(&(buf[108]))) << 32);
  i->atime = le2be(&(buf[8]));
  i->ctime = le2be(&(buf[12]));
  i->mtime = le2be(&(buf[16]));
  i->dtime = le2be(&(buf[20]));
  i->gid = le2be16(&(buf[24]));
  i->nlinks = le2be16(&(buf[26]));
  i->nsectors = le2be(&(buf[28]));
  i->flags = le2be(&(buf[32]));
  i->os_res_1 = le2be(&(buf[36]));
  i->dbptr0 = le2be(&(buf[40]));
  i->dbptr1 = le2be(&(buf[44]));
  i->dbptr2 = le2be(&(buf[48]));
  i->dbptr3 = le2be(&(buf[52]));
  i->dbptr4 = le2be(&(buf[56]));
  i->dbptr5 = le2be(&(buf[60]));
  i->dbptr6 = le2be(&(buf[64]));
  i->dbptr7 = le2be(&(buf[68]));
  i->dbptr8 = le2be(&(buf[72]));
  i->dbptr9 = le2be(&(buf[76]));
  i->dbptr10 = le2be(&(buf[80]));
  i->dbptr11 = le2be(&(buf[84]));
  i->sibptr = le2be(&(buf[88]));
  i->dibptr = le2be(&(buf[92]));
  i->tibptr = le2be(&(buf[96]));
  i->ngen = le2be(&(buf[100]));
  i->file_acl = le2be(&(buf[104]));
  i->dir_acl = le2be(&(buf[108]));
  i->frag_blk = le2be(&(buf[112]));
  for (int j = 0; j < 12; j++) {
    i->os_res_2[j] = buf[116+j];
  }
}

/*
  get an inode off the disk and into the inode table.
  the only field that needs to be valid in the pointed-to
  entry is the number. everything else will be initialized
  TODO: use a block cache
*/
int read_ino(struct ino *i, struct e2fs_fsinfo *fi) {
  uint8_t *sec_buf = kmalloc(512);
  if (sec_buf == NULL) {
    putstr("Could not allocate sec_buf in load_ino\r\n");
    return -1;
  }

  // calculate position on disk
  uint32_t ino_bgrp, ino_tbl_blk, ino_index, ino_blk;
  ino_bgrp = (i->ino_num - 1) / fi->sblk->inos_p_bgrp;
  ino_tbl_blk = fi->bgdt[ino_bgrp].ino_tbl;
  ino_index = (i->ino_num - 1) % fi->sblk->inos_p_bgrp;
  ino_blk = (ino_index * fi->sblk->ino_sz) / fi->blk_sz;
  uint32_t ino_blk_rem = (ino_index * fi->sblk->ino_sz) % fi->blk_sz;
  uint32_t ino_lba = fi->vol_start + (((ino_tbl_blk + ino_blk) * fi->blk_sz) / 512);
  ino_lba += ino_blk_rem / 512;
  
  read_sector(ino_lba, sec_buf);
  
  uint32_t offs = ino_index & 1 ? 256 : 0;
  // the cost of living in a dystopian little-endian world
  byteswap_ino(i, sec_buf + offs);
  i->refcount = 0;
  i->dirty = 0;
  
  kfree(sec_buf, 512);
  return 0;
}

/* get an inode from the table. if it is not present,
   load it from disk into an empty slot.
   based off minix 1 code (c) Prentice Hall 1987, 1997
*/
struct ino *get_ino(uint32_t ino_num, struct e2fs_fsinfo *fi) {
  struct ino *i, *f = NULL;

  for (i = &inotab[0]; i < &inotab[N_INO]; i++) {
    if (i->refcount > 0) { // in use
      if (i->ino_num == ino_num) {
	i->refcount++;
	return i;
      }
    } else { // free slot, remember in case needed
      f = i;
    }
  }
  
  // if we got to here the required inode isn't in the table.
  // see if free slot is available.
  if (f == NULL) {
    putstr("Out of available inode slots\r\n");
    return NULL;
  }
  
  f->ino_num = ino_num;
  read_ino(f, fi);
  return f;
}

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

void sector_dump(uint32_t lba) {
  uint8_t *buf = kmalloc(512);
  if (buf == NULL) {
    putstr("Could not allocate buffer in sector_dump\r\n");
    return;
  }
  read_sector(lba, buf);
  hexdump(buf, 512);
  kfree(buf, 512);
}

void init_fdtab() {
  memset(&fdtab, 0, sizeof(fdtab));
}

struct __attribute__((__packed__)) elf32_elfheader {
  uint8_t magid[16]; // magic number and system info
  uint16_t type;
  uint16_t isa;
  uint32_t version;
  uint32_t prg_entry_offs;
  uint32_t phdr_offs;
  uint32_t shdr_offs;
  uint32_t flags;
  uint16_t hdr_sz;
  uint16_t phdr_ent_sz;
  uint16_t phdr_n_ents;
  uint16_t shdr_ent_sz;
  uint16_t shdr_n_ents;
  uint16_t shdr_strtab_idx;
};

struct __attribute__((__packed__)) elf32_phdr {
  uint32_t seg_type;
  uint32_t p_offset;
  uint32_t p_vaddr;
  uint32_t p_paddr;
  uint32_t p_filesz;
  uint32_t p_memsz;
  uint32_t flags;
  uint32_t align;
};

enum elf32_section_type {
  SHT_NULL, SHT_PROGBITS, SHT_SYMTAB, SHT_STRTAB, SHT_RELA,
  SHT_HASH, SHT_DYNAMIC, SHT_NOTE, SHT_NOBITS, SHT_REL, SHT_SHLIB,
  SHT_DYNSYM
};

const char *elf32_stype_names[] = { "NULL", "PROGBITS", "SYMTAB",
  "STRTAB", "RELA", "HASH", "DYNAMIC", "NOTE", "NOBITS", "REL", "SHLIB",
  "DYNSYM"
};

struct __attribute__((__packed__)) elf32_shdr {
  uint32_t name_idx;
  uint32_t sec_type;
  uint32_t flags;
  uint32_t addr;
  uint32_t offs;
  uint32_t size;
  uint32_t link;
  uint32_t info;
  uint32_t align;
  uint32_t ent_sz;
  uint8_t pad[0x14];
};

int main() {
  init_mem();
  //wouldn't be necessary if i had a proper crt0
  memset(&inotab, 0, sizeof(inotab));
  uint8_t *sec_buf = kmalloc(512);
  if (sec_buf == NULL) {
    putstr("Could not allocate sec_buf in main\r\n");
    return -1;
  }
  
  struct e2fs_fsinfo *fi = init_fsinfo();
  if (fi == NULL) {
    putstr("fsinfo initialization failed\r\n");
    return -1;
  }

  if (fi->sblk->fs_state == 1)
    putstr("File system clean.\r\n");
  else
    putstr("File system has errors.\r\n");

  putstr("Total blocks: ");
  prlong(fi->sblk->n_blks);
  putstr("\r\n");

  putstr("Total inodes: ");
  prlong(fi->sblk->n_inos);
  putstr("\r\n");

  putstr("Block size: ");
  prlong(fi->blk_sz);
  putstr("\r\n");

  putstr("Blocks per block group: ");
  prlong(fi->sblk->blks_p_bgrp);
  putstr("\r\n");

  putstr("Inodes per block group: ");
  prlong(fi->sblk->inos_p_bgrp);
  putstr("\r\n");

  putstr("Total block groups: ");
  prlong(fi->n_bgrp);
  putstr("\r\n");

  putstr("Version: ");
  prlong((uint32_t)(fi->sblk->version >> 32));
  putch('.');
  prlong((uint32_t)(fi->sblk->version & 0xffffffff));
  putstr("\r\n");
  
  if (fi->sblk->version >= 0x0000000100000000) {
    putstr("Extended fields:\r\n");
    putstr("Features: (Req.-Opt.-R/O) ");
    prlong(fi->sblk->req_features);
    putch('-');
    prlong(fi->sblk->opt_features);
    putch('-');
    prlong(fi->sblk->ro_features);
    putstr("\r\n");

    putstr("Volume name: ");
    putstr(fi->sblk->volume_name);
    putstr("\r\n");
  }

  putstr("Inode size: ");
  prlong(fi->sblk->ino_sz);
  putstr("\r\n");

  uint32_t root_ino = 2;
  struct ino *root = get_ino(root_ino, fi);

  uint32_t dbptr0 = root->dbptr0;
  read_sector(BLK_LBA(dbptr0, fi->vol_start, fi->blk_sz), sec_buf);
  print_dir(sec_buf);
  kfree(root, sizeof(struct ino));
  uint32_t elf_ino = find_name(sec_buf, "mm.elf");
  if (elf_ino == 0) {
    putstr("Couldn't find mm.elf\r\n");
  } else {
    putstr("mm.elf inode number: ");
    prlong(elf_ino);
    putstr("\r\n");
  }

  struct ino *elf = get_ino(elf_ino, fi);
  dbptr0 = elf->dbptr0;
  putstr("mm.elf size: ");
  prlong((uint32_t)(elf->size & 0xffffffff));
  putstr("\r\n");
  read_sector(BLK_LBA(dbptr0, fi->vol_start, fi->blk_sz), sec_buf);
  struct elf32_elfheader eh;
  copymem(sec_buf, (uint8_t*)&eh, 52);
  if (eh.magid[0] != 0x7f || eh.magid[1] != 'E' || eh.magid[2] != 'L' || eh.magid[3] != 'F') {
    putstr("Bad ELF header or error reading file\r\n");
    return -1;
  }
  putstr("ELF");
  if (eh.magid[4] == 1)
    putstr("32");
  else if (eh.magid[4] == 2)
    putstr("64");
  if (eh.magid[5] == 1)
    putstr("LE\r\n");
  else if (eh.magid[5] == 2)
    putstr("BE\r\n");

  putstr("ELF header size: ");
  prword(eh.hdr_sz);
  putstr("\r\n");

  putstr("Number of program header entries: ");
  prword(eh.phdr_n_ents);
  putstr("\r\n");

  putstr("Number of section header entries: ");
  prword(eh.shdr_n_ents);
  putstr("\r\n");

  putstr("Size of section header entries: ");
  prword(eh.shdr_ent_sz);
  putstr("\r\n");

  putstr("Program header table offset: ");
  prlong(eh.phdr_offs);
  putstr("\r\n");

  putstr("Section header table offset: ");
  prlong(eh.shdr_offs);
  putstr("\r\n");
  
  struct elf32_phdr ph;
  copymem(sec_buf+eh.phdr_offs, (uint8_t*)&ph, eh.phdr_ent_sz);

  putstr("Program segment 0:\r\n");
  putstr("Segment type: ");
  switch (ph.seg_type) {
  case 0x00:
    putstr("NULL");
    break;
  case 0x01:
    putstr("LOAD");
    break;
  case 0x02:
    putstr("DYNAMIC");
    break;
  case 0x03:
    putstr("INTERP");
    break;
  case 0x04:
    putstr("NOTE");
    break;
  default:
    putstr("Unknown");
    break;
  }
  putstr("\r\n");
  putstr("Offset: ");
  prlong(ph.p_offset);
  putstr(" VADDR: ");
  prlong(ph.p_vaddr);
  putstr(" PADDR: ");
  prlong(ph.p_paddr);
  putstr("\r\nFile size: ");
  prlong(ph.p_filesz);
  putstr(" Memory size: ");
  prlong(ph.p_memsz);
  putstr(" Alignment: ");
  prlong(ph.align);
  putstr("\r\n");

  struct elf32_shdr *shdrs = kmalloc(eh.shdr_ent_sz * eh.shdr_n_ents);
  if (shdrs == NULL) {
    putstr("Couldn't allocate section header array\r\n");
    return -1;
  }
  copymem(sec_buf+eh.shdr_offs, (uint8_t*)shdrs, eh.shdr_ent_sz * eh.shdr_n_ents);

  for (int i = 0; i < eh.shdr_n_ents; i++) {
    putstr("Section ");
    prlong((uint32_t)i);
    putstr(":\r\n");
    putstr("Type: ");
    prlong(shdrs[i].sec_type);
    putstr("\r\n");
  }
  
  return 0;
}
    
