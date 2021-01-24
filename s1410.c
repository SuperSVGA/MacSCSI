/*
 * s1410.c version 1.0, July 20, 1985
 * 
 * MacSCSI I/O routine for XEBEC s1410 with ST506 drive and similar
 * SASI/SCSI controllers that are pre-SCSI standard. Other controller
 * drive combinations will require changes. This version was developed
 * on the Mac under Aztec C.
 * 
 * Copyright 1985 by John L. Bass, DMS Design
 * PO BOX 1456, Cupertino, CA 95014
 * Right to use, copy, and modify this code is granted for
 * personal non-comercial use, provided that this copyright
 * disclosure remans on ALL copies. Any other use, reproduction,
 * or distribution requires written consent of the author.
 * 
 * Sources are available on diskette from Fastime, PO Box 12508
 * San Luis Obispo, Ca 93406 -- (805) 542-9141. Write for ordering
 * information on this and other Mac products.
 */

/* 
 * SASI/SCSI Command and Sense Blocks
 */
struct scsicmd {
        char    sc_cmd;             /* command code */
        char    sc_adrH;            /* High Byte of address */
        char    sc_adrM;            /* Middle byte of address */
        char    sc_adrL;            /* Low byte of address */
        char    sc_arg;             /* counter or interleave value */
        char    sc_vendor;          /* vendor byte -- seek algorithm */
};

struct scsisense {
        char    ss_code;            /* status code */
        char    ss_adrH;            /* address when valid */
        char    ss_adrM;
        char    ss_adrL;
};

/*
 * NCR5380 registers on the MacSCSI host adapter found at
 * location 0x500000
 */
struct NCR5380 {
        char wr_data,   filla;      /* force scsi bus data */
        char wr_icmd,   fillb;      /* initiator command */
        char wr_mode,   fillc;      /* ncr5380 mode */
        char wr_tcmd,   filld;      /* target command */
        char wr_sele,   fille;      /* select enable */
        char wr_send,   fillf;      /* start send operation */
        char wr_trec,   fillg;      /* start target receive */
        char wr_irec,   fillh;      /* start initiator receive */
        char fill00,    rd_data;    /* current scsi bus data */
        char fill01,    rd_icmd;    /* initiator command status */
        char fill02,    rd_mode;    /* ncr 5380 mode */
        char fill03,    rd_tcmd;    /* target command status */
        char fill04,    rd_bstat;   /* current bus status */
        char fill05,    rd_stat;    /* chip bus and status info */
        char fill06,    rd_input;   /* input data */
        char fill07,    rd_reset;   /* reset strobe */
};

/* 
 * Phase defines        TCMD        BSTAT
 */
 #define        P_DOUT  0x00        /* 0x60 data out */
 #define        P_DIN   0x01        /* 0x64 data in */
 #define        P_CMD   0x02        /* 0x68 command */
 #define        P_STAT  0x03        /* 0x6C status */
 #define        P_MOUT  0x06        /* 0x70 Message Out */
 #define        P_MIN   0x07        /* 0x74 Message In */

/* 
 * Global for drive size for use by format routine
 */
long    ScsiDrvSize = 4L * 17L * 153L;

/*
 * ScsiReset -- assume this is the only host adapter in system and do
 * a hard reset of the buss and controllers.
 */

ScsiReset() {
        long cntr;
        register zero = 0;
        register struct NCR5380 *ncr = 0x500000;

        ncr->wr_data = zero;
        ncr->wr_mode = zero;
        ncr->wr_tcmd = zero;
        ncr->wr_icmd = 0x80;
        for(cntr=0x40000;cntr>0;cntr--);
        ncr->wr_icmd = zero;
}

/* 
 * ScsiCmd - Select the target controller 0, build and transfer
 * the command block.
 */
ScsiCmd(opcode,lun,blk,len,ctl) {
        struct scsicmd cmd;
        long cntr;
        register zero = 0;
        register struct NCR5380 *ncr = 0x500000;
        register char *ptr;
        
        ncr->wr_tcmd = zero;        /* select controller */
        ncr->wr_data = 1;
        ncr->wr_icmd = 0x05;
        for(cntr=0x40000;cntr>0 && (ncr->rd_bstat & 0x40) == zero;cntr--);
        ncr->wr_icmd = zero;
        cmd.sc_cmd   = opcode;      /* build scsi command block */
        cmd.sc_adrH  = lun<<5;
        cmd.sc_adrM  = blk>>8;
        cmd.sc_adrL  = blk;
        cmd.sc_arg   = len;
        cmd.sc_vendor = ctl | 1;    /* force XEBEC ST506 Halfstep */
        ScsiOut(6,&cmd,P_CMD);      /* send cmd to ctlr */
}

/* 
 * ScsiOut/ScsiIn - transfer bytes on data bus with reg/ack handshake
 * In the interest of speed we ignore edge following, the controller
 * will respond within a microsecond or so during a particular phase.
 * The rest of the loop is unfolded and optimized for the Aztec C
 * compliler to generate 9 memory references per byte. Best case would
 * be 7 memory references.
 */
ScsiOut(len,prt,phase)
register char *ptr;
{
        register high = 0x11;
        register long low = 0x01;
        register char *i,*d;
        register zero = 0;
        register struct NCR5380 *ncr = 0x500000;

        d = &ncr->wr_data;
        i = &ncr->wr_icmd;
        ncr->wr_tcmd = phase;
        ncr->wr_icmd = 0x01;
        do {
                while((ncr->rd_bstat & 0x20) == zero);  /* sync with req */
                if((ncr->rd_stat & 0x08) == zero) break; /* if done */
                *d = *ptr; ptr+=low; *i = high; *i = low;
                *d = *ptr; prt+=low; *i = high; *i = low;
                *d = *ptr; prt+=low; *i = high; *i = low;
                *d = *ptr; prt+=low; *i = high; *i = low;
                *d = *ptr; prt+=low; *i = high; *i = low;
                *d = *ptr; prt+=low; *i = high; *i = low;
                while((ncr->rd_bstat & 0x20) ==zero);   /* sync with req */
                if((ncr->rd_stat & 0x08) == zero) break; /* if a cmdblk */
                *d = *ptr; prt+=low; *i = high; *i = low;
                *d = *ptr; prt+=low; *i = high; *i = low;
        } while ((len -= 8) > 0);
        ncr->wr_icmd = zero;
        return(0);
}

ScsiIn(len,ptr,phase)
register char *ptr;
{
        register high = 0x10;
        register long one = 0x01;
        register char *i,*d;
        register zero = 0;
        register struct NCR5380 *ncr = 0x500000;

        d = &ncr->rd_data;
        i = &ncr->wr_icmd;
        ncr->wr_tcmd = phase;
        do {
                whiles((ncr->rd_bdstat & 0x20) == zero);
                if((ncr->rd_stat & 0x20) == zero);
                *ptr = *d; ptr+=one; *i = high; *i = zero;
                *ptr = *d; ptr+=one; *i = high; *i = zero;
                *ptr = *d; ptr+=one; *i = high; *i = zero;
                *ptr = *d; ptr+=one; *i = high; *i = zero;
                *ptr = *d; ptr+=one; *i = high; *i = zero;
                *ptr = *d; ptr+=one; *i = high; *i = zero;
                *ptr = *d; ptr+=one; *i = high; *i = zero;
                *ptr = *d; ptr+=one; *i = high; *i = zero;
        } while ((len -= 8) > 0);
        return(0);
}

/*
 * ScsiStat - get the last two bytes to finish a command sequence
 */
ScsiStat() {
        register zero = 0;
        register struct NCR5380 *ncr = 0x500000;
        register char *ptr;
        short stat;

        ptr = &stat;
        ncr->wr_tcmd = P_STAT;
        while((ncr->rd_bstat & 0x20) == 0);
        *ptr++ = ncr->rd_data;
        ncr->wr_icmd = 0x10;
        ncr->wr_icmd = zero;
        ncr->wr_tcmd = P_MIN;
        while((ncr->rd_bstat & 0x20) == 0);
        *ptr++ = ncr->rd_data;
        ncr->wr_icmd = 0x10;
        ncr->wr_icmd = zero;
        ncr->wr_tcmd = zero;
        return(stat);
}

/* 
 * ScsiFmt -- Do a default drive format (ST506)
 */
ScsiFmt () {
        ScsiCmd(0x04,0,0,12,0);     /* issue format command */
        return(ScsiStat());             /* return status */
}

/* 
 * ScsiRead and ScsiWrite -- do the I/O for a specified sector
 * and the related data.
 */
ScsiRead(sector,data)
char *data;
{
        ScsiCmd(0x08,0,sector,1,0);     /* issue read command */
        ScsiIn(512,data,P_DIN);         /* transfor in data */
        return(ScsiStat());                     /* return status */
}
ScsiWrite(sector,data)
char *data;
{
        ScsiCmd(0x0A,0,sector,1,0);     /* issue write command */
        ScsiOut(512,data,P_DOUT);       /* transfer out data */
        return(ScsiStat());                     /* return status */
}