/*
 *			P R O T O C O L . H
 *
 *  Definitions pertaining to the Remote RT protocol.
 *  
 *  Author -
 *	Michael John Muuss
 *
 *  Source -
 *	SECAD/VLD Computing Consortium, Bldg 394
 *	The U. S. Army Ballistic Research Laboratory
 *	Aberdeen Proving Ground, Maryland  21005
 *  
 *  Distribution Status -
 *	Public Domain, Distribution Unlimitied.
 *
 *  $Header$
 */

/* For use in MSG_VERSION exchanges */
#define PROTOCOL_VERSION	\
	"$Header$"

#define MSG_MATRIX	2
#define MSG_OPTIONS	3
#define MSG_LINES	4	/* request pixel interval be computed */
#define MSG_END		5
#define MSG_PRINT	6
#define MSG_PIXELS	7	/* response to server MSG_LINES */
#define MSG_RESTART	8	/* recycle server */
#define MSG_LOGLVL	9	/* enable/disable logging */
#define	MSG_CD		10	/* change directory */
#define MSG_CMD		11	/* server sends command to dispatcher */
#define MSG_VERSION	12	/* server sends version to dispatcher */
#define MSG_DB		13	/* request/send database lines 16k max */
#define MSG_CHECK	14	/* check database files for match */
#define MSG_DIRBUILD		15	/* request rt_dirbuild() be called */
#define MSG_DIRBUILD_REPLY	16	/* response to MSG_DIRBUILD */
#define MSG_GETTREES		17	/* request rt_gettrees() be called */
#define MSG_GETTREES_REPLY	18	/* response to MSG_GETTREES */

#define REMRT_MAX_PIXELS	(32*1024)	/* Max MSG_LINES req */

/*
 *  This structure is used for MSG_PIXELS messages
 */
struct line_info  {
	int	li_startpix;
	int	li_endpix;
	int	li_frame;
	int	li_nrays;
	double	li_cpusec;
	double	li_percent;	/* percent of system actually consumed */
	/* A scanline is attached after here */
};

#define LINE_O(x)	offsetof(struct  line_info, x)

struct bu_structparse desc_line_info[] =  {
	{"%d", 1, "li_startpix", LINE_O(li_startpix),	FUNC_NULL },
	{"%d", 1, "li_endpix",	LINE_O(li_endpix),	FUNC_NULL },
	{"%d", 1, "li_frame",	LINE_O(li_frame),	FUNC_NULL },
	{"%d", 1, "li_nrays",	LINE_O(li_nrays),	FUNC_NULL },
	{"%f", 1, "li_cpusec",	LINE_O(li_cpusec),	FUNC_NULL },
	{"%f", 1, "li_percent",	LINE_O(li_percent),	FUNC_NULL },
	{"",	0,			0 }
};
