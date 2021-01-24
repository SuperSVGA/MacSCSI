/* 
 * MacSCSI non-partitioned format routine.
 * This verision was developed on the Mac under Aztec C.
 * 
 * Copyright 1985 by John L. Bass, DMS Design
 * PO Box 1456, Cupertino, CA 95014
 * Right to use, copy, and modify this code is granted for
 * personal non-comercial use, provided that this copyright
 * disclosure remains on ALL copies, Any other use, reproduction,
 * or distribution requires the written consent of the author.
 * 
 * Sources are available on diskette from Fastime, PO Box 12508
 * San Luis Obispo, Ca 93406 -- (805) 546-9141. Write for ordering
 * information on this and other Mac products.
 */

struct Volume {
                short   drSigWord;      /* should be $D2D7 */
                long    drCrDate;       /* date and time of initialization */
                long    drLsBkUp;       /* date and time of last backup */ 
                short   drAtrb;         /* volume attributes */
                short   drNumFls;       /* # of files in file directory */
                short   drDirSt;        /* first logical block of file dir */
                short   drBlLen;        /* # of logical blocks in file dir */
                short   drNmAlBlocks;   /* # of allocation blocks on volume */
                long    drAlBlkSiz;     /* size of allocation blocks */
                long    drClpSiz;       /* # of bytes to allocate */
                short   drAlBlSt;       /* logical block number of first
                                                            allocation block */
                long    drNxtFnum;      /* next unused file number */
                short   drFreeBks;      /* # of unused allocation blocks */
                char    drVn;           /* length of volume name */
                char    drFill[512-37]; /* volume name & start of alloc. map */
};
struct  Volume v;

char    zeros[512];                             /* block worth of nulls */

long    ScsiDrvSize;                    /* number of 512 byte sectors */

main() {
                char *p;
                int     i;
#ifdef hack
        printf("Scsi reset\n");
        ScsiReset();
        printf("Drive being formatted\n");
        if(ScsiFmt()) {
                printf("Format Failed\n");
                exit(1);
        }
#endif
        v.drSigWord = 0xd2d7;
        v.drCrDate  = v.drLsBkUp = 0;
        v.drAtrb    = 0;
        v.drNumFls  = 0;
        v.drNxtFnum = 1;
        v.drDirSt   = 4;
        printf("Directory Start:    %6.6d\n",v.drDirSt);

        v.drBlLen   = 28;
        printf("Directory BlkLen:   %6.6d\n",v.drBlLen);

        v.drAlBlSt = v.drDirSt + v.drBlLen;
        printf("First Allocation Blk: %6.6d\n",v.drAlBlSt);

        v.drAlBlkSiz = 512L * (ScsiDrvSize/640L + 1L);
        v.drClpSiz  = v.drAlBlkSiz;
        printf("Allocation Blocks:  %6.6ld\n",v.drNmAlBlocks);

        v.drNmAlBlocks = (ScsiDrvSize-v.drAlBlSt)/(v.drAlBlkSiz>>9);
        v.drFreeBks = v.drNmAlBlocks;
        printf("Allocation Blocks:  %6.6d\n",v.drNmAlBlocks);

        for(v.drVN = 0,p="MacSCSI";*p;p++) v.drFill[v.drVN++] = *p;

        printf("Initializing Drive\n");
        ScsiWrite(0,zeros);
        ScsiWrite(1,zeros);

        if(ScsiWrite(2,&v)) {
                printf("Write of config block failed\n");
        }
        for(i=3;i<ScsiDrvSize;i++) if(ScsiWrite(i,zeros)) {
                printf("Write failed at block %d\n",i);
        }
        printf("Checking Drive\n");
        for(i=0;i<ScsiDrvSize;i++) if(ScsiRead(i,zeros)) {
                printf("Read failed at block %d\n",i);
        }
        printf("Format completed ok\n");
}
main ()
{
        printf("Exit code %d\n",OpenDriver("\P.MacSCSI"));
}

main()
{
        *(int *)0x210 = 5;              /* boot drive to MacSCSI */
}