  align 2
  section code,code
  org $000800

CF_BASE equ $180000

CF_DATA equ CF_BASE+0
CF_ERR equ CF_BASE+3
CF_FEATURE equ CF_BASE+3
CF_SEC_COUNT equ CF_BASE+5
CF_SEC_NUM equ CF_BASE+7
CF_LBA_7_0 equ CF_BASE+7
CF_CYL_LOW equ CF_BASE+9
CF_LBA_15_8 equ CF_BASE+9
CF_CYL_HIGH equ CF_BASE+11
CF_LBA_23_16 equ CF_BASE+11
CF_HEAD equ CF_BASE+13
CF_LBA_27_24 equ CF_BASE+13
CF_STAT equ CF_BASE+15
CF_CMD equ CF_BASE+15

CF_STAT_ERR equ $01
CF_STAT_COR equ $04
CF_STAT_DRQ equ $08
CF_STAT_DSC equ $10
CF_STAT_DWF equ $20
CF_STAT_RDY equ $40
CF_STAT_BSY equ $80

CF_IDENTIFY equ $ec
CF_READ_SEC equ $20
CF_WRITE_SEC equ $30

  move.b #0, CF_SEC_COUNT
  move.b #0, CF_LBA_7_0
  move.b #0, CF_LBA_15_8
  move.b #0, CF_LBA_23_16
  move.b #CF_IDENTIFY, CF_CMD
  jsr cf_drq_wait

  move.l #(512/2)-1, d1
  movea.l #$020000, a1
.read:
  move.w CF_DATA, (a1)+
  jsr cf_busy_wait
  dbra d1, .read

  move.b #$01, CF_FEATURE
  move.b #$ef, CF_CMD
  jsr cf_busy_wait
  move.b CF_ERR, $030000

  movea.l #$020000, a0
  move.l #$2bf, d0
  jsr cf_read_sector_8

  move.b #'X', $020000
  move.b #'Q', $020043

  movea.l #$020000, a0
  move.l #$2bf, d0
  jsr cf_write_sector_8

  rts

cf_busy_wait:
  btst.b #7, CF_STAT
  bne cf_busy_wait
  rts

cf_drq_wait:
  btst.b #3, CF_STAT
  beq cf_drq_wait
  rts
  
  ; a0: buffer
  ; d0: lba
cf_read_sector:	
  movem.l a0-1/d0-d1, -(sp)
  
  rol.w #8, d0
  swap d0
  rol.w #8, d0 ; byte swap
  or.b #$e0, d0 ; head bits in lba reg
  move.l #CF_BASE, a1
  movep.l d0, CF_LBA_7_0-CF_BASE(a1)
  move.b #1, CF_SEC_COUNT
  move.b #CF_READ_SEC, CF_CMD
  jsr cf_busy_wait
  jsr cf_drq_wait
  move.l #(512/2)-1, d1 ; moving words, dbra quits at negative
.readloop:
  move.w CF_DATA, d0
  ror.w #8, d0 ; byte swap
  move.w d0, (a0)+
  dbra d1, .readloop
  
  movem.l (sp)+, a0-a1/d0-d1
  rts

cf_read_sector_8:
  movem.l a0-1/d0-d1, -(sp)
  
  rol.w #8, d0
  swap d0
  rol.w #8, d0 ; byte swap
  or.b #$e0, d0 ; head bits in lba reg
  move.l #CF_BASE, a1
  movep.l d0, CF_LBA_7_0-CF_BASE(a1)
  move.b #1, CF_SEC_COUNT
  move.b #CF_READ_SEC, CF_CMD
  jsr cf_busy_wait
  jsr cf_drq_wait
  move.l #512-1, d1 ; dbra quits at negative
.readloop:
  move.b CF_DATA+1, (a0)+ ; need lds asserted for cs so read from odd
  dbra d1, .readloop
  
  movem.l (sp)+, a0-a1/d0-d1
  rts

  ; a0: buffer
  ; d0: lba
cf_write_sector:	
  movem.l a0-1/d0-d1, -(sp)
  
  rol.w #8, d0
  swap d0
  rol.w #8, d0 ; byte swap
  or.b #$e0, d0 ; head bits in lba reg
  move.l #CF_BASE, a1
  movep.l d0, CF_LBA_7_0-CF_BASE(a1)
  move.b #1, CF_SEC_COUNT
  move.b #CF_WRITE_SEC, CF_CMD
  jsr cf_busy_wait
  jsr cf_drq_wait
  move.l #(512/2)-1, d1 ; moving words, dbra quits at negative
.writeloop:
  move.w (a0)+, d0
  ror.w #8, d0 ; byte swap
  move.w d0, CF_DATA
  jsr cf_busy_wait
  dbra d1, .writeloop
  
  movem.l (sp)+, a0-a1/d0-d1
  rts

cf_write_sector_8:	
  movem.l a0-1/d0-d1, -(sp)
  
  rol.w #8, d0
  swap d0
  rol.w #8, d0 ; byte swap
  or.b #$e0, d0 ; head bits in lba reg
  move.l #CF_BASE, a1
  movep.l d0, CF_LBA_7_0-CF_BASE(a1)
  move.b #1, CF_SEC_COUNT
  move.b #CF_WRITE_SEC, CF_CMD
  jsr cf_busy_wait
  jsr cf_drq_wait
  move.l #512-1, d1 ; dbra quits at negative
.writeloop:
  move.b (a0)+, CF_DATA+1
  jsr cf_busy_wait
  dbra d1, .writeloop
  
  movem.l (sp)+, a0-a1/d0-d1
  rts
