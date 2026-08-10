  align 2
  ; ram 00000000-000fffff
  ; duart 00100000-007fffff
  ; rom 00800000-ffffffff

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

CR equ $0d
LF equ $0a
BS equ $08

linebuf equ $00400
blkbuf equ $00400
jifs equ $0600
secs equ $0601
mins equ $0602
hrs equ $0603
  section code, code
  org $00fc0000
  
vectors:
  dc.l $000ffffe ; ssp at reset
  dc.l _start ; reset
  dc.l berr_trap ; berr
  dc.l addr_trap ; addr error
  dc.l illegal_instr_trap ; illegal instr
  dc.l div_trap ; div by zero
  dc.l chk_trap ; chk
  dc.l generic_trap ; trapv
  dc.l generic_trap ; privilege violation
  dc.l trace_trap ; trace
  dc.l generic_trap ; line 1010
  dc.l generic_trap ; line 1111
  dc.l reserved_trap ; res
  dc.l reserved_trap ; res
  dc.l reserved_trap ; format error (68010)
  dc.l uninit_vec_trap ; uninitialized vector
  dc.l reserved_trap ; res
  dc.l reserved_trap ; res
  dc.l reserved_trap ; res
  dc.l reserved_trap ; res
  dc.l reserved_trap ; res
  dc.l reserved_trap ; res
  dc.l reserved_trap ; res
  dc.l reserved_trap ; res
  dc.l spurious_trap ; spurious irq
  dc.l auto_trap ; l1 autovector
  dc.l auto_trap ; l2 autovector
  dc.l auto_trap ; l3 autovector
  dc.l auto_trap ; l4 autovector
  dc.l auto_trap ; l5 autovector
  dc.l auto_trap ; l6 autovector
  dc.l auto_trap ; l7 autovector
  dcb.l 16, trap_inst
  dcb.l 16, reserved_trap
  dcb.l 192, generic_trap
  
_start:	
  move.b #$70, DUART_ACR ; baud rate set 1, ext /16 no ip irq
  move.b #$cc, DUART_CSRB ; 38.4k
  move.b #$13, DUART_MR1B ; 8n
  move.b #$07, DUART_MR2B ; 1 stop
  move.b #$02, DUART_CTUR
  move.b #$3d, DUART_CTLR ; ~100hz
  move.b #$40, DUART_IVR
  move.b #$08, DUART_IMR ; timer irq enable
  move.b DUART_START_CTR, d0 ; read-activated, no value returned
  move.b #$05, DUART_CRB ; enable tx and rx
  move.l #$00000000, jifs

copyvecs:
  move.l #255, d0
  lea vectors, a0
  movea #$00000, a1
.loop:
  move.l (a0)+, (a1)+
  dbra d0, .loop
  lea timer_handler, a0
  move.l a0, $000100
  lea trap0_handler, a0
  move.l a0, $000080
  move.w #$2400, sr ; int level 0 (duart is 5)

  lea str_ready, a0
  bsr puts
ready:
  move.b #']', d0
  bsr putchar
  bsr readline
  bsr toupper
  bsr parseline
  jmp ready

  
readline:
  move.l #linebuf, a0
.loop:
  jsr getchar_b
  cmp.b #CR, d0
  bne .character
  cmp.l #linebuf, a0
  beq .loop ; empty line, ignore
  clr.b (a0) ; null terminate
  move.b #CR, d0
  jsr putchar
  move.b #LF, d0
  jsr putchar
  rts
.character:
  cmp.b #BS, d0
  bne .notbs
  cmp.l #linebuf, a0
  beq .skip
  subq #1, a0
  jsr putchar ; delete char from buffer and send bs to terminal
.skip:
  jmp .loop
.notbs:
  cmp.b #$1f, d0
  bls .loop
  cmp.b #$7f, d0
  bhi .loop ; ignore unprintable characters
  jsr putchar ; echo it
  move.b d0, (a0)+
  bra .loop

toupper:
  move.l #linebuf, a0
.loop:
  move.b (a0), d0
  tst.b d0
  beq .quit
  cmp.b #'z', d0
  bhi .notletter
  cmp.b #'a'-1, d0
  bls .notletter
  subi.b #$20, d0
  move.b d0, (a0)
.notletter:
  addq #1, a0
  bra .loop
.quit:
  rts
  
parseline:
  move.l #linebuf, a0
  move.b (a0), d0
  jsr isdigit
  bcs .addr
  cmp.b #'X', d0
  bne bad_input
  jsr do_xmodem
  bra .quit
.addr:
  jsr getaddr
  bcc bad_input
  move.b (a0)+, d0 ; d0 now holds first non digit character, analyze it
  cmp.b #':', d0
  beq do_poke
  cmp.b #'G', d0
  beq do_run
  cmp.b #'U', d0
  beq do_dbg
  cmp.b #'.', d0
  bne .skiprange
  exg a1, a2
  jsr getaddr
  bcc bad_input
  exg a2, a1
  jsr peek_range
  bra .quit
.skiprange:
  jsr peek
.quit:
  rts

bad_input:
  lea str_err_bad_input, a0
  jsr puts
  rts

peek: ; addr in a1
  move.l a1, d0
  jsr prlong
  move.b #':', d0
  jsr putchar
  move.b #' ', d0
  jsr putchar
  move.b (a1), d0
  jsr prbyte
  move.b #CR, d0
  jsr putchar
  move.b #LF, d0
  jsr putchar
  rts

peek_range: ; a1 start, a2 end
  move.l a1, d0
  jsr prlong
  move.b #':', d0
  jsr putchar
  move.b #' ', d0
  jsr putchar
.outer:
  moveq #15, d1
.inner:
  move.b (a1)+, d0
  jsr prbyte
  cmpa.l a2, a1
  bhi .quit
  move.b #' ', d0
  jsr putchar
  dbra d1, .inner
  move.b #CR, d0
  jsr putchar
  move.b #LF, d0
  jsr putchar
  moveq #9, d2
.pad:
  move.b #' ', d0
  jsr putchar
  dbra d2, .pad
  bra .outer
.quit:
  move.b #CR, d0
  jsr putchar
  move.b #LF, d0
  jsr putchar
  rts

do_run:
  jsr (a1)
  rts

do_dbg:
  ori.w #$8000, sr ; trace on
  jsr (a1)
  rts

do_poke:
  move.b (a0)+, d0
  cmp.b #' ', d0
  bne .skip
.space:
  move.b (a0)+, d0
.skip:
  jsr isdigit
  bcc bad_input
  exg d0, d1
  move.b (a0)+, d0
  jsr isdigit
  bcc bad_input
  exg d1, d0
  jsr asc2byte
  move.b d0, (a1)+
  move.b (a0)+, d0
  cmp.b #' ', d0
  bne .quit
  bra .space
.quit:
  rts
  
getaddr: ; convert ascii addr from (a0) until first non digit, return a1
  clr.l d2 ; temp storage to manipulate addr
  moveq #3, d3 ; counter
.loop:
  move.b (a0), d0
  jsr isdigit
  bcc .done ; first is already checked to be a digit by caller
  ; if we are out of digits after a multiple of 2 then it
  ; was a short address rather than a typo (probably)
  addq #1, a0
  move.b (a0)+, d1
  exg d0, d1
  jsr isdigit
  bcc .err
  lsl.l #8, d2
  exg d1, d0
  jsr asc2byte
  move.b d0, d2
  dbra d3, .loop
.done:
  movea.l d2, a1
  ori #$01, ccr
  rts
.err:
  andi #$fe, ccr
  rts

asc2nyb: ; ascii in d0, return in low half of d0
  subi #'0', d0
  cmp.b #$09, d0
  bls .skip
  subq #$07, d0
.skip:
  andi.b #$0f, d0
  rts

asc2byte: ; most significant d0 least d1
  jsr asc2nyb
  lsl.b #4, d0
  exg d0, d1
  jsr asc2nyb
  or.b d0, d1
  exg d1, d0
  rts
  
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

prword:	; print d0.w
  move.l d0, -(sp)
  rol.l #8, d0
  rol.l #8, d0
  rol.l #8, d0
  jsr prbyte
  rol.l #8, d0
  jsr prbyte
  move.l (sp)+, d0
  rts
  
getchar_b: ; char in d0, blocking
  btst.b #0, DUART_SRB
  beq getchar_b
  move.b DUART_RBB, d0
  rts

getchar_timeout: ; carry set if no char after ~1s
  move.l d1, -(sp)
  move.b jifs, d0
  move.b secs, d1
.loop:
  btst.b #0, DUART_SRB
  bne .break
  cmp.b secs, d1
  beq .loop
  cmp.b jifs, d0
  bne .loop
  move.l (sp)+, d1
  ori.b #1, ccr
  rts
.break:
  move.b DUART_RBB, d0
  move.l (sp)+, d1
  andi.b #$fe, ccr
  rts

isdigit: ; char in d0, carry set if true
  cmp.b #'0'-1, d0
  bls .bad
  cmp.b #'F', d0
  bhi .bad
  cmp.b #'9', d0
  bls .good
  cmp.b #'A'-1, d0 ; between 9 and a = bad
  bls .bad
.good:
  ori #$01, ccr ; sec
  rts
.bad:
  andi #$fe, ccr ; clc
  rts

do_xmodem:
  lea str_xmodem_start, a0
  jsr puts
  movea.l #$000800, a2 ; destination
  move.w #1, d1 ; block number
.open_conn:
  move.b #'C', d0
  jsr putchar
  jsr getchar_timeout
  bcs .open_conn
  cmp.b #$01, d0 ; SOH
  bne .open_conn
  bra .skip_soh
.get_block:
  jsr getchar_timeout
  bcs .retry
  cmp.b #$01, d0
  beq .skip_soh
  cmp.b #$04, d0 ; EOT
  beq .done
  jsr retry_block
  bra .get_block
  
.skip_soh:
  movea.l #blkbuf, a1
  jsr getchar_timeout ; block num
  bcs .retry
  move.b d0, (a1)+
  jsr getchar_timeout ; negated block num
  bcs .retry
  move.b d0, (a1)+

.loop:
  jsr getchar_timeout
  bcs .retry
  move.b d0, (a1)+
  cmp.l #blkbuf+$82, a1 ; 128 data bytes + blknums
  bne .loop
  
  jsr getchar_timeout ; crc lo
  bcs .retry
  lsl.w #8, d0
  jsr getchar_timeout ; crc hi
  bcs .retry
  move.w d0, (a1)+
  exg d0, d2 ; crc backup
  clr.w d3

.validate_block:
  movea.l #blkbuf, a1
  cmp.b (a1)+, d1 ; d1 = blknum
  bne .retry
  move.b d1, d5
  eori.b #$ff, d1
  cmp.b (a1)+, d1
  bne .retry
  move.b d5, d1
.chkloop:
  move.b (a1)+, d0
  jsr update_crc
  cmp.l #blkbuf+$82, a1
  bne .chkloop
  cmp.w (a1)+, d3 ; crc
  bne .retry

  sub.l #$82, a1
  move.w #($80/4)-1, d4
.copy:
  move.l (a1)+, (a2)+
  dbra d4, .copy

  move.b #$06, d0 ; ACK
  jsr putchar
  addq.b #1, d1
  bra .get_block

.done:
  move.b #$06, d0
  jsr putchar
  lea str_xmodem_finish, a0
  jsr puts
  rts

.err:
  jsr xmodem_purge
  move.b #$18, d0 ; CAN
  jsr putchar
  lea str_err_xmodem, a0
  jsr puts
  rts

.retry:
  jsr retry_block
  bra .get_block

xmodem_purge:
  jsr getchar_timeout
  bcc xmodem_purge
  rts
  
retry_block:
  jsr xmodem_purge
  move.b #$15, d0 ; NAK
  jsr putchar
  rts

update_crc: ; byte in d0
  lsl.w #8, d0
  move.w #7, d4
  eor.w d0, d3
.rotloop:
  lsl.w #1, d3
  bcc .clear
  eori.w #$1021, d3
.clear:
  dbra d4, .rotloop
  rts
  
generic_trap:
  movem.l a0/d0, -(sp)
  lea str_exception, a0
  jsr puts
  lea str_exc_generic, a0
  jsr puts
  movem.l (sp)+, a0/d0
  rte

berr_trap:
  movem.l a0/d0, -(sp)
  lea str_exception, a0
  jsr puts
  lea str_exc_berr, a0
  jsr puts
  movem.l (sp)+, a0/d0
  rte

addr_trap:
  movem.l a0/d0, -(sp)
  lea str_exception, a0
  jsr puts
  lea str_exc_addr, a0
  jsr puts
  movem.l (sp)+, a0/d0
  rte

illegal_instr_trap:
  movem.l a0/d0/d1, -(sp)
  lea str_exception, a0
  jsr puts
  lea str_exc_ill, a0
  jsr puts
  move.l 14(sp), d0
  movea.l d0, a0
  move.l (a0), d1
  jsr prlong
  move.b #':', d0
  jsr putchar
  move.b #' ', d0
  jsr putchar
  exg d0, d1
  jsr prlong
  movem.l (sp)+, a0/d0/d1
  rte

uninit_vec_trap:
  movem.l a0/d0, -(sp)
  lea str_exception, a0
  jsr puts
  lea str_exc_uninit, a0
  jsr puts
  movem.l (sp)+, a0/d0
  rte

div_trap:
  movem.l a0/d0, -(sp)
  lea str_exception, a0
  jsr puts
  lea str_exc_div, a0
  jsr puts
  movem.l (sp)+, a0/d0
  rte

chk_trap:
  movem.l a0/d0, -(sp)
  lea str_exception, a0
  jsr puts
  lea str_exc_chk, a0
  jsr puts
  movem.l (sp)+, a0/d0
  rte

trace_trap:
  movem.l d0-d7/a0-a7, -(sp)
  jmp debug_entry

reserved_trap:
  movem.l a0/d0, -(sp)
  lea str_exception, a0
  jsr puts
  lea str_exc_reserved, a0
  jsr puts
  movem.l (sp)+, a0/d0
  rte

trap_inst:
  movem.l a0/d0, -(sp)
  lea str_exception, a0
  jsr puts
  lea str_exc_trap, a0
  jsr puts
  movem.l (sp)+, a0/d0
  rte

auto_trap:
  movem.l a0/d0, -(sp)
  lea str_exception, a0
  jsr puts
  lea str_exc_auto, a0
  jsr puts
  movem.l (sp)+, a0/d0
  rte

spurious_trap:
  movem.l a0/d0, -(sp)
  lea str_exception, a0
  jsr puts
  lea str_exc_spurious, a0
  jsr puts
  movem.l (sp)+, a0/d0
  rte

timer_handler:
  movem.l d0/a0, -(sp)
  lea jifs, a0
  addq.b #1, (a0)
  cmp.b #100, (a0)
  bne .done
  clr.b (a0)
  addq.b #1, (1,a0)
  cmp.b #60, (1,a0)
  bne .done
  clr.b (1,a0)
  addq.b #1, (2,a0)
  cmp.b #60, (2,a0)
  bne .done
  clr.b (2,a0)
  addq.b #1, (3,a0)
.done:
  move.b DUART_STOP_CTR, d0 ; clear isr flag
  movem.l (sp)+, d0/a0
  rte

trap0_handler: ; soft reset
  movea.l #$000ffffe, sp
  move.b #CR, d0
  jsr putchar
  move.b #LF, d0
  jsr putchar
  jmp ready

str_ready:
  dc.b "Ready", CR, LF, $00
str_xmodem_start:
  dc.b "Receiving Xmodem file...", CR, LF, $00
str_xmodem_finish:
  dc.b "Successfully received Xmodem file", CR, LF, $00
str_err_xmodem:
  dc.b "Error receiving Xmodem file", CR, LF, $00
str_err_bad_input:
  dc.b "ERR: Bad input", CR, LF, $00
str_exception:
  dc.b "******** EXCEPTION ********", CR, LF, $00
str_exc_berr:
  dc.b "Bus error", CR, LF, $00
str_exc_addr:
  dc.b "Unaligned address", CR, LF, $00
str_exc_div:
  dc.b "Divide by zero", CR, LF, $00
str_exc_chk:
  dc.b "Bounds check failure", CR, LF, $00
str_exc_ill:
  dc.b "Illegal instruction", CR, LF, $00
str_exc_uninit:
  dc.b "Uninitialized vector", CR, LF, $00
str_exc_generic:
  dc.b "No unique handler implemented", CR, LF, $00
str_exc_reserved:
  dc.b "Reserved exception", CR, LF, $00
str_exc_trap:
  dc.b "TRAP instruction", CR, LF, $00
str_exc_auto:
  dc.b "Autovector interrupt", CR, LF, $00
str_exc_spurious:
  dc.b "Spurious IRQ", CR, LF, $00

  ;-------------------------
  ; debugger
  ;-------------------------

  align 2
debug_entry:
  ; stack frame: $00-$3f d0-d7/a0-a7
  ; $00 d0 $04 d1 $08 d2 $0c d3
  ; $10 d4 $14 d5 $18 d6 $1c d7
  ; $20 a0 $24 a1 $28 a2 $2c a3
  ; $30 a4 $34 a5 $38 a6 $3c a7
  ; $40 sr $42 pc $46 fmt + vector
  move.l $42(sp), d0
  jsr prlong
  move.b #':', d0
  jsr putchar
  move.b #' ', d0
  jsr putchar
  movea.l $42(sp), a0
  move.l (a0), d0
  jsr prlong
.prompt:
  move.b #CR, d0
  jsr putchar
  move.b #LF, d0
  jsr putchar
  move.b #'*', d0
  jsr putchar
  jsr getchar_b
  cmp.b #'n', d0
  beq .next
  cmp.b #'q', d0
  beq .quit
  cmp.b #'d', d0
  bne .prompt
.regdump:
  movea.l sp, a0
  move.l a0, d1
  addi.l #$20, d1
  movea.l d1, a1
  move.w #7, d1
  move.b #$30, d2
.loop:
  move.b #'d', d0
  jsr putchar
  move.b d2, d0
  jsr putchar
  move.b #' ', d0
  jsr putchar
  move.l (a0)+, d0
  jsr prlong
  move.b #9, d0 ; htab
  jsr putchar
  move.b #'a', d0
  jsr putchar
  move.b d2, d0
  jsr putchar
  move.b #' ', d0
  jsr putchar
  move.l (a1)+, d0
  jsr prlong
  move.b #CR, d0
  jsr putchar
  move.b #LF, d0
  jsr putchar
  addq.b #1, d2
  dbra d1, .loop
  move.b #'s', d0
  jsr putchar
  move.b #'r', d0
  jsr putchar
  move.b #' ', d0
  jsr putchar
  move.w $40(sp), d0
  jsr prword
  move.b #9, d0 ; htab
  jsr putchar
  move.b #'p', d0
  jsr putchar
  move.b #'c', d0
  jsr putchar
  move.b #' ', d0
  jsr putchar
  move.l $42(sp), d0
  jsr prlong
  move.b #9, d0 ; htab
  jsr putchar
  move.b #'u', d0
  jsr putchar
  move.b #'s', d0
  jsr putchar
  move.b #'p', d0
  jsr putchar
  move.b #' ', d0
  jsr putchar
  move.l usp, a2
  move.l a2, d0
  jsr prlong
  move.b #CR, d0
  jsr putchar
  move.b #LF, d0
  jsr putchar
  bra .prompt
.next:
  movem.l (sp)+, d0-d7/a0-a7
  rte
.quit:
  andi.w #$7fff, $40(sp) ; no more trace after rte
  movem.l (sp)+, d0-d7/a0-a7
  rte
  
  dcb.b $01000000-*, $ff
