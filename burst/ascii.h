/*
	Author:		Gary S. Moss
			U. S. Army Ballistic Research Laboratory
			Aberdeen Proving Ground
			Maryland 21005-5066

	$Header$
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
