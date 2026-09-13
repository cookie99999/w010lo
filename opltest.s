  align 2
  section code,code
  org $000800
  
OPL_BASE equ $fa3001
OPL_REG_ADDR equ OPL_BASE
OPL_STAT equ OPL_BASE
OPL_REG_VALUE equ OPL_BASE+2

DUART_BASE equ $00fa1001
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


  clr.w running
  clr.w counter
  move.b #$00, DUART_IMR
  lea vgm_irq, a0
  move.l a0, $000100
  move.b #$60, DUART_ACR ; timer mode x1 no div 16
  move.b #$29, DUART_CTLR
  move.b #$00, DUART_CTUR ; close enough to 44.1khz
  move.b DUART_START_CTR, d0
  move.b #$08, DUART_IMR
  
  jmp skip
  move.b #$f4, d1
clearloop:
  move.b d1, OPL_REG_ADDR
  bsr delay
  move.b #$00, OPL_REG_VALUE
  dbra d1, clearloop	

  move.b #$20, OPL_REG_ADDR
  bsr delay
  move.b #$01, OPL_REG_VALUE
  bsr delay
  move.b #$40, OPL_REG_ADDR
  bsr delay
  move.b #$10, OPL_REG_VALUE
  bsr delay
  move.b #$60, OPL_REG_ADDR
  bsr delay
  move.b #$f0, OPL_REG_VALUE
  bsr delay
  move.b #$80, OPL_REG_ADDR
  bsr delay
  move.b #$77, OPL_REG_VALUE
  bsr delay
  move.b #$a0, OPL_REG_ADDR
  bsr delay
  move.b #$98, OPL_REG_VALUE
  bsr delay
  move.b #$23, OPL_REG_ADDR
  bsr delay
  move.b #$01, OPL_REG_VALUE
  bsr delay
  move.b #$43, OPL_REG_ADDR
  bsr delay
  move.b #$00, OPL_REG_VALUE
  bsr delay
  move.b #$63, OPL_REG_ADDR
  bsr delay
  move.b #$f0, OPL_REG_VALUE
  bsr delay
  move.b #$83, OPL_REG_ADDR
  bsr delay
  move.b #$77, OPL_REG_VALUE
  bsr delay
  move.b #$b0, OPL_REG_ADDR
  bsr delay
  move.b #$31, OPL_REG_VALUE

skip:
  lea vgm_data, a6
  move.l $34(a6), d0
  ror.w #8, d0
  swap d0
  ror.w #8, d0
  add.l #$34, d0
  move.l a6, d1
  add.l d1, d0
  move.l d0, vgm_offs
  clr.w counter
  move.w #1, running

wait:
  tst.w running
  bne wait
  rts

delay:
  move.l #$14, d2
inner:
  nop
  dbra d2, inner
  rts

vgm_irq: ; more or less copied from aslak
  tst.w running
  bne @skip
  move.b d0, -(sp)
  move.b DUART_STOP_CTR, d0
  move.b (sp)+, d0
  rte
@skip:
  movem.l d0/a0, -(sp)
  move.w counter, d0
  beq @notcounting
  subq.w #1, d0
  move.w d0, counter
  bra @quit2
@notcounting:
  move.l vgm_offs, a0
@loop:
  move.b (a0)+, d0
  cmpi.b #$5a, d0
  beq @ym_cmd
  cmpi.b #$61, d0
  beq @countn
  cmpi.b #$62, d0
  beq @count735
  cmpi.b #$63, d0
  beq @count882
  cmpi.b #$00, d0
  beq @eof
  cmpi.b #$66, d0
  beq @eof
@quit1:
  move.l a0, vgm_offs
@quit2:
  move.b DUART_STOP_CTR, d0 ; reset irq flag
  movem.l (sp)+, d0/a0
  rte

@ym_cmd:
  move.b (a0)+, OPL_REG_ADDR
  move.w #$30, d0
@delay1:
  dbra d0, @delay1
  move.b (a0)+, OPL_REG_VALUE
  move.w #$30, d0
@delay2:
  dbra d0, @delay2
  bra @loop

@countn:
  move.b (a0)+, d0
  lsl.w #8, d0
  move.b (a0)+, d0
  ror.w #8, d0
  ;lsr.w #4, d0
  move.w d0, counter
  bra @quit1

@count735:
  move.w #735, counter
  bra @quit1

@count882:
  move.w #882, counter
  bra @quit1

@eof:
  clr.w running
  bra @quit1

running:
  dcb.w 1
counter:
  dcb.w 1
vgm_offs:	
  dcb.l 1

vgm_data:
  incbin test2.vgm
