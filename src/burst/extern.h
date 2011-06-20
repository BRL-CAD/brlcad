/*                        E X T E R N . H
 * BRL-CAD
 *
 * Copyright (c) 2004-2011 United States Government as represented by
 * the U.S. Army Research Laboratory.
 *
 * This program is free software; you can redistribute it and/or
 * modify it under the terms of the GNU Lesser General Public License
 * version 2.1 as published by the Free Software Foundation.
 *
 * This program is distributed in the hope that it will be useful, but
 * WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
 * Lesser General Public License for more details.
 *
 * You should have received a copy of the GNU Lesser General Public
 * License along with this file; see the file named COPYING for more
 * information.
 *
 */
/** @file burst/extern.h
 *
 */

#ifndef __EXTERN_H__
#define __EXTERN_H__

#include "common.h"

#include <stdarg.h>
#include <stdlib.h>
#include <string.h>

#include "bio.h"
#include "fb.h"
#include "bu.h"

#include "./burst.h"
#include "./trie.h"
#include "./Hm.h"

/* External variables from termlib. */
extern char *CS, *DL;
extern int CO, LI;

/* External functions from application. */
extern Colors *findColors();
extern Func *getTrie();
extern Trie *addTrie();
extern int chkEntryNorm();
extern int chkExitNorm();
extern int closFbDevice();
extern int imageInit();
extern int initUi();
extern int openFbDevice();
extern int findIdents();
extern int readColors();
extern int readIdents();
extern int notify();
extern int roundToInt();
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
extern void prntScr(const char *, ...);
extern void brst_log(const char *, ...);
extern void warning();
extern void prntUsage();
extern void clr_Tabs();
extern void prntShieldComp();
extern void qFree();
extern void prntRegionHdr();
extern int qAdd();
extern void prntSeg();
extern void gridRotate();
extern void HmBanner();
extern void spallInit();
extern void readBatchInput();
extern void set_Cbreak();
extern void clr_Echo();
extern void clr_Tabs();
extern void reset_Tty();
extern void save_Tty();

extern void (*norml_sig)(), (*abort_sig)();
extern void abort_RT();
extern void intr_sig();

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

extern int batchmode;
extern int cantwarhead;
extern int deflectcone;
extern int dithercells;
extern int fatalerror;
extern int groundburst;
extern int reportoverlaps;
extern int reqburstair;
extern int shotburst;
extern int tty;
extern int userinterrupt;

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

#endif  /* __EXTERN_H__ */

/*
 * Local Variables:
 * mode: C
 * tab-width: 8
 * indent-tabs-mode: t
 * c-file-style: "stroustrup"
 * End:
 * ex: shiftwidth=4 tabstop=8
 */
