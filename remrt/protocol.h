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
#define PROTOCOL_VERSION	\
	"$Header$"

#define MSG_START	1
#define MSG_MATRIX	2
#define MSG_OPTIONS	3
#define MSG_LINES	4
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

/*
 *  This structure is used for MSG_PIXELS messages
 */
struct line_info  {
	int	li_len;
	int	li_startpix;
	int	li_endpix;
	int	li_frame;
	int	li_nrays;
	double	li_cpusec;
	double	li_percent;	/* percent of system actually consumed */
	/* A scanline is attached after here */
};

#define LINE_O(x)	offsetof(struct  line_info, x)

struct imexport desc_line_info[] =  {
	"len",		LINE_O(li_len),		1,
	"%d",		LINE_O(li_startpix),	1,
	"%d",		LINE_O(li_endpix),	1,
	"%d",		LINE_O(li_frame),	1,
	"%d",		LINE_O(li_nrays),	1,
	"%f",		LINE_O(li_cpusec),	1,
	"%f",		LINE_O(li_percent),	1,
	"",		0,			0
};
