/*
	Author:		Gary S. Moss
			U. S. Army Ballistic Research Laboratory
			Aberdeen Proving Ground
			Maryland 21005-5066
			(301)278-6651 or AV-298-6651

	$Header$ (BRL)
 */
#include "string.h"
#include "machine.h"
#include "fb.h"
#include "./burst.h"
#include "./trie.h"
#include "Hm/Hm.h"
extern Func	*getTrie();
extern Trie	*addTrie();
extern bool	InitUi();
extern char	*malloc();
extern void	closeUi();
extern void	rt_log();

/* External functions from C library. */
extern char	*tmpnam();
extern char	*malloc();
extern char	*getenv();
extern char	*sbrk();
extern char	*strncpy();
#ifdef SYSV
extern long	lrand48();
#else
extern long	random();
#endif

/* External variables from termlib. */
extern char	*CS, *DL;
extern int	CO, LI;

/* External functions from application. */
extern Colors	*findColors();

extern bool	findIdents();
extern bool	readColors();
extern bool	readIdents();

#if defined( SYSV )
extern void	(*norml_sig)(), (*abort_sig)();
extern void	abort_RT();
extern void	intr_sig();
extern void	stop_sig();
#else
extern int	(*norml_sig)(), (*abort_sig)();
extern int	abort_RT();
extern int	intr_sig();
extern int	stop_sig();
#endif

extern void	exitCleanly();
extern void	gridInit();
extern void	gridModel();
extern void	locPerror();
extern void	logCmd();
extern void	plotGrid();
extern void	plotInit();
extern void	plotPartition();
extern void	plotRay();
extern void	plotShieldComp();
extern void	prntAspectInit();
extern void	prntBurstHdr();
extern void	prntCellIdent();
extern void	prntFusingComp();
extern void	prntGridOffsets();
extern void	prntIdents();
extern void	prntPhantom();
extern void	prntRayIntersect();
extern void	prntScr();
extern void	prntTimer();
extern void	readCmdFile();

extern Colors	colorids;
extern FBIO	*fbiop;
extern FILE	*gridfp;
extern FILE	*histfp;
extern FILE	*outfp;
extern FILE	*plotfp;
extern FILE	*shotfp;
extern FILE	*tmpfp;
extern HmMenu	*mainhmenu;
extern Ids	airids;
extern Ids	armorids;
extern Ids	critids;
extern Trie	*cmdtrie;

extern bool	batchmode;
extern bool	cantwarhead;
extern bool	deflectcone;
extern bool	dithercells;
extern bool	fatalerror;
extern bool	reportoverlaps;
extern bool	tty;
extern bool	userinterrupt;

extern char	airfile[];
extern char	armorfile[];
extern char	cmdbuf[];
extern char	cmdname[];
extern char	colorfile[];
extern char	critfile[];
extern char	errfile[];
extern char	fbfile[];
extern char	gedfile[];
extern char	gridfile[];
extern char	histfile[];
extern char	objects[];
extern char	outfile[];
extern char	plotfile[];
extern char	scrbuf[];
extern char	scriptfile[];
extern char	shotfile[];
extern char	title[];
extern char	timer[];
extern char	tmpfname[];

extern char	*cmdptr;

extern char	**template;

extern fastf_t	bdist;
extern fastf_t	burstpoint[];
extern fastf_t	cellsz;
extern fastf_t	conehfangle;
extern fastf_t	fire[];
extern fastf_t	modlcntr[];
extern fastf_t	pitch;
extern fastf_t	raysolidangle;
extern fastf_t	setback;
extern fastf_t	standoff;
extern fastf_t	unitconv;
extern fastf_t	viewazim;
extern fastf_t	viewelev;
extern fastf_t	viewsize;
extern fastf_t	yaw;

extern int	co, li;
extern int	firemode;
extern int	gridsz;
extern int	gridxfin;
extern int	gridyfin;
extern int	gridxorg;
extern int	gridyorg;
extern int	nbarriers;
extern int	noverlaps;
extern int	nprocessors;
extern int	nriplevels;
extern int	nspallrays;
extern int	units;
extern int	zoom;

extern struct rt_i	*rtip;
