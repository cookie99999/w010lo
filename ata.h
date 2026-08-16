#ifndef __ATA_H

#include <stdint.h>
void ata_identify(uint8_t *buf);
void read_sector(uint32_t lba, uint8_t *buf);

#define __ATA_H
#endif
