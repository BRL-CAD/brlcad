/*
	Author:		Gary S. Moss
			U. S. Army Ballistic Research Laboratory
			Aberdeen Proving Ground
			Maryland 21005-5066
			(301)278-6647 or AV-298-6647
*/
/*
	Originally extracted from SCCS archive:
		SCCS id:	@(#) ascii.h	1.8
		Last edit: 	4/24/86 at 13:11:47
		Retrieved: 	4/24/86 at 13:14:58
		SCCS archive:	/vld/moss/src/fbed/s.ascii.h
 */
#define NUL		'\000'
#define SOH		'\001'
#define	STX		'\002'
#define INTR		'\003'
#define	EOT		'\004'
#define ACK		'\006'
#define BEL		'\007'
#define	BS		'\010'
#define HT		'\011'
#define LF		'\012'
#define FF		'\014'
#define	CR		'\015'
#define DLE		'\020'
#define	DC1		'\021'
#define	DC2		'\022'
#define	DC3		'\023'
#define	DC4		'\024'
#define	KILL		'\025'
#define	CAN		'\030'
#define	ESC		'\033'
#define	GS		'\035'
#define	RS		'\036'
#define	US		'\037'
#define SP		'\040'
#define DEL		'\177'

#define Ctrl(chr)	((int)chr&037)
