/*
	Author:		Gary S. Moss
			U. S. Army Ballistic Research Laboratory
			Aberdeen Proving Ground
			Maryland 21005-5066

	$Header$ (BRL)
 */

#include "conf.h"

#if defined(HAVE_STDARG_H)
# include <stdarg.h>
#else
#include <varargs.h>
#endif


#include <string.h>
#include "machine.h"
#include "fb.h"

#include "./burst.h"
#include "./trie.h"
#include "./Hm.h"

/* External functions from C library. */
#if (__STDC__ && ! apollo) || (sgi && mips && BRLCAD4_0)
#include <stdlib.h>
extern pointer sbrk( int );
#else
extern char *fgets();
extern char *getenv();
extern char *malloc();
extern char *tmpnam();
extern pointer sbrk();
#endif
#ifdef SYSV
extern long	lrand48();
#else
extern long	random();
#endif

/* External variables from termlib. */
extern char *CS, *DL;
extern int CO, LI;

/* External functions from application. */
extern Colors *findColors();
extern Func *getTrie();
extern Trie *addTrie();
extern bool chkEntryNorm();
extern bool chkExitNorm();
extern bool closFbDevice();
extern bool imageInit();
extern bool initUi();
extern bool openFbDevice();
extern bool findIdents();
extern bool readColors();
extern bool readIdents();
extern int notify();
extern int round();
extern void closeUi();
extern void colorPartition();
extern void exitCleanly();
extern void freeIdents();
extern void getRtHitNorm();
extern void gridInit();
extern void gridModel();
extern void gridToFb();
extern void locPerror();
extern void logCmd();
extern void paintCellFb();
extern void paintGridFb();
extern void paintSpallFb();
extern void plotGrid();
extern void plotInit();
extern void plotPartition();
extern void plotRay();
extern void prntAspectInit();
extern void prntBurstHdr();
extern void prntCellIdent();
extern void prntDbgPartitions();
extern void prntFiringCoords();
extern void prntFusingComp();
extern void prntGridOffsets();
extern void prntIdents();
extern void prntPhantom();
extern void prntRayHeader();
extern void prntRayIntersect();
extern void prntTimer();
extern void prompt();
extern void readCmdFile();
#if defined(HAVE_STDARG_H)
extern void prntScr( char *, ...);
extern void bu_log( char *, ... );
#else
extern void bu_log();
extern void prntScr();
#endif
extern void warning();

#if STD_SIGNAL_DECLS
extern void (*norml_sig)(), (*abort_sig)();
extern void abort_RT();
extern void intr_sig();
#else
extern int (*norml_sig)(), (*abort_sig)();
extern int abort_RT();
extern int intr_sig();
#endif

extern Colors colorids;
extern FBIO *fbiop;
extern FILE *burstfp;
extern FILE *gridfp;
extern FILE *histfp;
extern FILE *outfp;
extern FILE *plotfp;
extern FILE *shotfp;
extern FILE *shotlnfp;
extern FILE *tmpfp;
extern HmMenu *mainhmenu;
extern Ids airids;
extern Ids armorids;
extern Ids critids;
extern unsigned char *pixgrid;
extern unsigned char pixaxis[3];
extern unsigned char pixbkgr[3];
extern unsigned char pixbhit[3];
extern unsigned char pixblack[3];
extern unsigned char pixcrit[3];
extern unsigned char pixghit[3];
extern unsigned char pixmiss[3];
extern unsigned char pixtarg[3];
extern Trie *cmdtrie;

extern bool batchmode;
extern bool cantwarhead;
extern bool deflectcone;
extern bool dithercells;
extern bool fatalerror;
extern bool groundburst;
extern bool reportoverlaps;
extern bool reqburstair;
extern bool shotburst;
extern bool tty;
extern bool userinterrupt;

extern char airfile[];
extern char armorfile[];
extern char burstfile[];
extern char cmdbuf[];
extern char cmdname[];
extern char colorfile[];
extern char critfile[];
extern char errfile[];
extern char fbfile[];
extern char gedfile[];
extern char gridfile[];
extern char histfile[];
extern char objects[];
extern char outfile[];
extern char plotfile[];
extern char scrbuf[];
extern char scriptfile[];
extern char shotfile[];
extern char shotlnfile[];
extern char title[];
extern char timer[];
extern char tmpfname[];

extern char *cmdptr;

extern char **template;

extern fastf_t bdist;
extern fastf_t burstpoint[];
extern fastf_t cellsz;
extern fastf_t conehfangle;
extern fastf_t fire[];
extern fastf_t griddn;
extern fastf_t gridlf;
extern fastf_t gridrt;
extern fastf_t gridup;
extern fastf_t gridhor[];
extern fastf_t gridsoff[];
extern fastf_t gridver[];
extern fastf_t grndbk;
extern fastf_t grndht;
extern fastf_t grndfr;
extern fastf_t grndlf;
extern fastf_t grndrt;
extern fastf_t modlcntr[];
extern fastf_t modldn;
extern fastf_t modllf;
extern fastf_t modlrt;
extern fastf_t modlup;
extern fastf_t pitch;
extern fastf_t raysolidangle;
extern fastf_t standoff;
extern fastf_t unitconv;
extern fastf_t viewazim;
extern fastf_t viewelev;
extern fastf_t viewsize;
extern fastf_t yaw;
extern fastf_t xaxis[];
extern fastf_t zaxis[];
extern fastf_t negzaxis[];

extern int co;
extern int devhgt;
extern int devwid;
extern int firemode;
extern int gridsz;
extern int gridxfin;
extern int gridyfin;
extern int gridxorg;
extern int gridyorg;
extern int gridheight;
extern int gridwidth;
extern int li;
extern int nbarriers;
extern int noverlaps;
extern int nprocessors;
extern int nriplevels;
extern int nspallrays;
extern int units;
extern int zoom;

extern struct rt_i *rtip;
