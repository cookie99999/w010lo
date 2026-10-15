  align 2
  section code,code
  org $000800

DUART_BASE equ $fa1001
DUART_MR1A equ DUART_BASE
DUART_MR2A equ DUART_BASE
DUART_SRA equ DUART_BASE+2
DUART_CSRA equ DUART_BASE+2
DUART_CRA equ DUART_BASE+4
DUART_RBA equ DUART_BASE+6
DUART_TBA equ DUART_BASE+6
DUART_IPCR equ DUART_BASE+8
DUART_ACR equ DUART_BASE+8
DUART_ISR equ DUART_BASE+10
DUART_IMR equ DUART_BASE+10
DUART_CUR equ DUART_BASE+12
DUART_CTUR equ DUART_BASE+12
DUART_CLR equ DUART_BASE+14
DUART_CTLR equ DUART_BASE+14
DUART_MR1B equ DUART_BASE+16
DUART_MR2B equ DUART_BASE+16
DUART_SRB equ DUART_BASE+18
DUART_CSRB equ DUART_BASE+18
DUART_CRB equ DUART_BASE+20
DUART_RBB equ DUART_BASE+22
DUART_TBB equ DUART_BASE+22
DUART_IVR equ DUART_BASE+24
DUART_IP equ DUART_BASE+26
DUART_OPCR equ DUART_BASE+26
DUART_START_CTR equ DUART_BASE+28
DUART_OPR_SET equ DUART_BASE+28
DUART_STOP_CTR equ DUART_BASE+30
DUART_OPR_RESET equ DUART_BASE+30

CR equ $0d
LF equ $0a

start:
  movea.l ($000008), a1
  lea new_berr, a0
  move.l a0, $000008
  move.l a0, $0f0000
  movea.l #0, a0
  moveq.l #0, d1
loop:
  move.l (a0)+, d0
  tst.l d1
  beq loop
  exg d0, a0
  lea str_mem, a0
  jsr puts
  jsr prlong
  move.b #CR, d0
  jsr putchar
  move.b #LF, d0
  jsr putchar
  move.l a1, $000008
  rts

new_berr:
  ori.l #$ffffffff, d1
  ori.w #$8000, 8(sp) ; set rerun bit
  rte

putchar: ; char in d0
  btst.b #2, DUART_SRB
  beq putchar
  move.b d0, DUART_TBB
  rts

puts: ; ptr to null terminated str in a0
  move.b (a0)+, d0
  tst.b d0
  beq .quit
  jsr putchar
  bra puts
.quit:
  rts

prbyte:	; byte in d0
  move.b d0, -(sp)
  lsr.b #$04, d0
  andi.b #$0f, d0
  addi #$30, d0
  cmp.b #$39, d0 ; >'9'?
  bls .skip
  addq #$07, d0
.skip:
  jsr putchar
  move.b (sp)+, d0
  andi.b #$0f, d0
  addi #$30, d0
  cmp.b #$39, d0
  bls .skip2
  addq #$07, d0
.skip2:
  jsr putchar
  rts

prlong:	; print d0
  move.l d0, -(sp)
  rol.l #8, d0
  jsr prbyte
  rol.l #8, d0
  jsr prbyte
  rol.l #8, d0
  jsr prbyte
  rol.l #8, d0
  jsr prbyte
  move.l (sp)+, d0
  rts

str_mem:
  dc.b "Installed memory: ", $00
