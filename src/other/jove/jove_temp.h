/*
 *			J O V E _ T E M P . H
 *
 * $Revision$
 *
 * $Log$
 * Revision 1.1  2004/05/20 14:50:00  morrison
 * Sources that are external to BRL-CAD are moved from the top level to src/other/.
 *
 * Revision 11.2  1995/06/21 03:45:11  gwyn
 * Eliminated trailing blanks.
 * Eliminated blank line at EOF.
 *
 * Revision 11.1  95/01/04  10:35:23  mike
 * Release_4.4
 *
 * Revision 10.1  91/10/12  06:54:05  mike
 * Release_4.0
 *
 * Revision 2.4  91/08/30  19:33:24  mike
 * Changed VMUNIX to !defined(pdp11)
 *
 * Revision 2.3  91/08/30  17:39:55  mike
 * No symbols after #else
 *
 * Revision 2.2  87/04/14  20:07:34  dpk
 * Changed to support Crays.
 *
 * Revision 2.1  86/09/23  22:28:37  mike
 * Externs now declared properly.
 * I/O fixes for SysV
 *
 * Revision 2.1  86/09/23  22:26:34  mike
 * Externs now declared properly.
 * I/O fixes for SysV
 *
 * Revision 2.0  84/12/26  16:50:00  dpk
 * System as distributed to Berkeley 26 Dec 84
 *
 * Revision 1.2  83/12/16  00:10:22  dpk
 * Added distinctive RCS header
 *
 */

/*
 * The editor uses a temporary file for files being edited, in a structure
 * similar to that of ed.
 * Lines are represented in core by a pointer into the temporary file which
 * is packed into 16 bits.  15 of these bits index the temporary file,
 * the 16'th is used by by redisplay.  The parameters below control
 * how much the 15 bits are shifted left before they index the temp file.
 * Larger shifts give more slop in the temp file but allow larger files
 * to be edited.
 */

#if !defined(pdp11)

#define	BLKMSK	077777
#define	BNDRY	2
#define	INCRMT	02000
#define	LBTMSK	01776
#define	NMBLKS	077770
#define	OFFBTS	10
#define	OFFMSK	01777
#define	SHFT	0

#else

#define	BLKMSK	01777
#define	BNDRY	16
#define	INCRMT	0100
#define	LBTMSK	0760
#define	NMBLKS	1018
#define	OFFBTS	6
#define	OFFMSK	077
#define	SHFT	3

#endif

extern char	ibuff1[BSIZ],	/* Holds block `iblock1' of the tmp file */
	ibuff2[BSIZ],	/*   "     "   `iblock2' of the tmp file */
	obuff[BSIZ];	/* Holds the last block of the tmp file */
extern int	ichng1,		/* ibuff1 should be written to its
			 * blocks when it is used to read
			 * another block.
			 */
	ichng2,		/* "" */
	iblock1,	/* Block number of ibuff1 */
	iblock2,	/*		   ibuff2 */
	oblock,		/* 		   obuff  */
	nleft,		/* Number of good characters left in current block */
	hitin2,		/* Last read was in ibuff2 */
	tmpfd;
extern disk_line	tline;		/* Pointer to end of tmp file */

extern char	*tfname;
