/*
	SCCS id:	%Z% %M%	%I%
	Modified: 	%G% at %U%
	Retrieved: 	%H% at %T%
	SCCS archive:	%P%

	Author:	Gary S. Moss
	U. S. Army Ballistic Research Laboratory
	Aberdeen Proving Ground
	Maryland 21005-5066

*/
/**
	<Tc.h> -- MUVES "Tc" (terminal control) package definitions

	Rewritten to use BRL-CAD's libtermio
**/

/**

	The Tc package provides an interface to the terminal handler
	for controlling I/O processing.

**/
#ifndef Tc_H_INCLUDE
#define Tc_H_INCLUDE

#if 0

/**

	All of these functions take effect on the stream specified
	by fd.  Fd is assumed to be open.  This enables the application
	to control several terminal-style devices at once.

	void	TcSaveTty( int fd )

	Saves the current terminal modes.  This function should be
	called once at the beginning of the program to save the user's
	settings so that they can be restored via TcResetTty() when the
	program exits, is suspended via job control, or for whatever
	reason wishes to revert to the default terminal handler settings.

	void	TcResetTty( int fd )	restore the terminal modes

	void	TcClrCRNL( int fd )	turn off CR to NL mapping

	void	TcClrCbreak( int fd )	turn off cbreak mode

	void	TcClrEcho( int fd )	turn off echo mode

	void	TcClrRaw( int fd )	turn off raw mode

	void	TcClrTabs( int fd )	turn off tab expansion

	void	TcSetCbreak( int fd )	turn on cbreak mode

	void	TcSetEcho( int fd )	turn on echo mode

	void	TcSetHUPCL( int fd )	turn on "hang up on next close"

	void	TcSetRaw( int fd )	turn on raw mode

	void	TcSetTabs( int fd )	turn on tab expansion

**/
#if _STDC_
extern void	TcClrCRNL( int fd );
extern void	TcClrCbreak( int fd );
extern void	TcClrEcho( int fd );
extern void	TcClrRaw( int fd );
extern void	TcClrTabs( int fd );
extern void	TcResetTty( int fd );
extern void	TcSaveTty( int fd );
extern void	TcSetCbreak( int fd );
extern void	TcSetEcho( int fd );
extern void	TcSetHUPCL( int fd );
extern void	TcSetRaw( int fd );
extern void	TcSetTabs( int fd );
#else
extern void	TcClrCRNL();
extern void	TcClrCbreak();
extern void	TcClrEcho();
extern void	TcClrRaw();
extern void	TcClrTabs();
extern void	TcResetTty();
extern void	TcSaveTty();
extern void	TcSetCbreak();
extern void	TcSetEcho();
extern void	TcSetHUPCL();
extern void	TcSetRaw();
extern void	TcSetTabs();
#endif

#else

#define TcClrCRNL	clr_CRNL
#define TcClrCbreak	clr_Cbreak
#define TcClrEcho	clr_Echo
#define TcClrRaw	clr_Raw
#define TcClrTabs	clr_Tabs
#define TcResetTty	reset_Tty
#define TcSaveTty	save_Tty
#define TcSetCbreak	set_Cbreak
#define TcSetEcho	set_Echo
#define TcSetHUPCL	set_HUPCL
#define TcSetRaw	set_Raw
#define TcSetTabs	set_Tabs

#endif

#endif		/* Tc_H_INCLUDE */


