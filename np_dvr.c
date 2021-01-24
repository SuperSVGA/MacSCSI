/*
 * np_dvr.c version 1.0, July 20, 1985
 * MacSCSI non-partitioned driver routine.
 * This version was based on the ramdisk.asm example found in
 * Aztec C 1.06 and was developed on the Mac under Aztec C.
 *
 * The changes are Copyright 1985 by John L. Bass, DMS Design
 * PO Box 1456, Cupertino, CA 95014
 * Right to use, copy, and modify this code is granted for
 * personal non-commercial use, provided that this copyright
 * disclosure remains on ALL copies. Any other use, reproduction,
 * or distribution requires the written consent of the author.
 *
 * Sources are available on diskette from Fastime, PO Box 12508
 * San Luis Obispo, Ca 93406 -- (805) 546-9141. Write for ordering
 * information on this and other Mac products.
 */

#asm
;:ts=8
;
DRVNUM  equ     5               ;change this if conflicts with others
;
Start
        dc.w    $4f00           ;locked, read, write, ctrl, status
        dc.w    0               ;no delay
        dc.w    0               ;no events
        dc.w    0               ;no menu
        dc.w    open-Start      ;open routine
        dc.w    rdwrt-Start     ;prime routine
        dc.w    control-Start   ;control routine
        dc.w    status-Start    ;status routine
        dc.w    tst-Start       ;close routine
        dc.b    8
        dc.b    ".MacSCSI"      ;name of driver
        ds      0               ;for alignment
;
MAXMAP          equ      640
;
drBlLen         equ      16
drNmAlBlks      equ      18
drAlBlkSiz      equ      20
drClpSiz        equ      24
drAlBlSt        equ      28
drFreeBks       equ      34
;
template
        dc.w    $d2d7           ;signature word
        dc.l    0               ;init time
        dc.l    0               ;backup time
        dc.w    0               ;attributes
        dc.w    0               ;# of files
        dc.w    4               ;first dir block
        dc.w    0               ;# of dir blocks
        dc.w    0               ;# of allocation blocks total
        dc.l    0               ;bytes/allocation block
        dc.l    0               ;bytes/allocation call
        dc.w    4               ;first data block
        dc.l    0               ;next free file #
        dc.w    0               ;# of unused allocation block
        dc.b    7               ;length of name
        dc.b    "MacSCSI"       ;name of drive
        ds      0               ;for alignment
;
stabuf
        dc.w    0               ;current track
        dc.b    0               ;write protect in bit 7
        dc.b    1               ;disk in place
        dc.b    1               ;drive installed
        dc.b    0               ;sidedness of drive
        dc.l    0               ;next queue entry
        dc.w    0               ;unused
        dc.w    DRVNUM          ;drive number
drvref  dc.w    0               ;driver ref num
        dc.w    0               ;file system ID
        dc.b    0               ;sidedness of disk
        dc.b    0               ;needs flush flag
        dc.w    0               ;error count
;
diskbase_
        dc.l    0               ;base of disk memory
;
openflg
        dc.w    0               ;once only open flag
;
open
        lea     openflg,a0      ;get once only flag
        tst.w   (a0)            ;is it open already?
        bne     skip            ;yes -- just exit
        st      (a0)            ;set once only flag

        move.l  #14,d0          ;size of drive queue entry
        dc.w    $a51e           ;NewPtr - system heap
        clr.w   l0(a0)          ;local file system
        move.l  #DRVNUM,d0      ;drive number 5
        swap    d0
        move.w  24(a1),d0       ;driver reference number
        lea     drvref,a2       ;get address of driver reference number
        move.w  d0,(a2)         ;set up status buffer
        dc.w    $a04e           ;AddDriver

        move.l  (a1),-(sp)      ;push handle to resource
        dc.w    $a992           ;DetachResource

        link    a6,#-50         ;allocat the block on the stack
        move.l  sp,a0           ;setup as arg to mountvol trap
        move.w  #DRVNUM,22(a0)  ;set our drive number in block
        dc.w    $a00f           ;ask for it to be mounted
        unlk    a6              ;clear block from stack

skip
        move.l  #0,d0           ;set non-error return
        rts                     ;exit back to caller
;
rdwrt
        movem.l d0-d7/a0-a6,-(sp)       ;save regs
        move.l  a0,Pbp_
        move.l  a1,Dp_                          ;save arguments
        rts
restore_
        move.l  Dp_,a1                          ; pass DCEptr
        rts
#endasm

#include        <memory.h>
#include        <resource.h>
#include        <desk.h>
#include        <pb.h>
/*
 * variables used by asm routines
 * Dp and Pbp are device driver arguments handled by save/restore
 * ptr and dvrarg needed by AddDriver call
 */

DCEPtr          Dp;
ParmBlkPtr      Pbp;

/*
 * Local Stuff
 */

int     DvrState;
        jsr     ScsiRdWr_                ;Call the C stuff
        movem.l (sp)+,d0-d7/a0-a6        ;restore regs
        bra     tst                      ;exit via IOdone

control
        cmp.w   #1,26(a0)       ;is it KillIO
        bne.s   tst             ;no, exit
        move.l  #0,d0
        rts                     ;just return
;
status
        cmp.w   #8,26(a0)       ;is it status request??
        bne.s   tst
        movem.l a0/a1,-(sp)     ;save regs
        lea     28(a0),a1       ;get dest address
        lea     stabuf,a0
        move.l  #22,d0          ;move 22 bytes
        dc.w    $a02e           ;BlockMove
        movem.l (sp)+,a0/a1     ;restore regs
;
tst
        move.l  #0,d0                   ;okay return
        movem.l d4-d7/a4-a6,-(sp)       ;save regs going into IOdone
        move.l  $8fc,a0                 ;get IOdone address
        jsr     (a0)                    ;call IOdone
        movem.l (sp)+,d4-d7/a4-a6       ;restore them after IOdone
        rts                             ;and jump to it

        public  _Uend_,_Dorg_,_Cend_

save_
        lea     Start+(_Uend_-_Dorg_)+(_Cend_-Start),a4 ; setup baseadr
/*
 * ScsiRdWr -- do a driver read/write function.
 */
ScsiRdWr() {
        register struct ioParam *ip;
        long    len,part;
        short   blkno;
        char    *addr;
        save();
        ip = & Pbp->u.iop;
        part = Dp->dCtlPosition & 0xFFFFFE00;
        len = (ip->ioRegCount + 511) & 0xFFFFFE00;
        addr = ip->ioBuffer;
        while(len >= 512) {
                blkno = part>>9;
                if((Pbp->ioTrap & 0xff) == 2) {
                        ScsiRead(blkno, addr);
                 } else {
                        ScsiWrite(blkno, addr);
                }
                len -= 512;
                addr += 512;
                part += 512;
        }
        ip->ioActCount = ip->ioReqCount;
        restore();                      /* exit to I/O Done */
}