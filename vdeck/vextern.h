/*
	SCCS id:	@(#) vextern.h	2.4
	Last edit: 	12/20/85 at 19:03:47
	Retrieved: 	8/13/86 at 08:19:29
	SCCS archive:	/m/cad/vdeck/RCS/s.vextern.h

	Author:		Gary S. Moss
			U. S. Army Ballistic Research Laboratory
			Aberdeen Proving Ground
			Maryland 21005-5066
			(301)278-6647 or AV-298-6647
*/
#include <math.h>
#include "./ged_types.h"
#include "./3d.h"
#include "./vdeck.h"

extern double	unit_conversion;
extern int	debug;
extern char	*usage[], *cmd[];
extern mat_t	xform, notrans, identity;

extern Record record;
extern struct findrr findrr[];

extern char	dir_names[], *dir_last;

extern int	discr[];

extern char		*strcpy(), *strcat(), *mktemp();
extern char		*emalloc();
extern long		lseek();
extern void		abort_sig(), quit();
extern Directory	*lookup(), *diradd();
extern void		toc(), list_toc();
extern void		prompt();
extern void		exit(), free(), perror();

extern char	*toc_list[];
extern char	*curr_list[];
extern int	curr_ct;
extern char	*arg_list[];
extern int	arg_ct;
extern char	*tmp_list[];
extern int	tmp_ct;

extern int	objfd;		extern char	*objfile;
extern int	solfd;		extern char	st_file[];
extern int	regfd;		extern char	rt_file[];
extern int	ridfd;		extern char	id_file[];
extern int	idfd, rd_idfd;	extern char	disc_file[];
extern int	rrfd, rd_rrfd;	extern char	reg_file[];

extern int	ndir, nns, nnr, numrr;

extern int			regflag, orflag, delsol, delreg;
extern int			isave;
extern char			buff[],	name[];
extern char			operate;
extern long			savsol;
extern struct deck_ident	d_ident, idbuf;

#include <setjmp.h>
extern jmp_buf		env;
#define EPSILON		0.0001
#define CONV_EPSILON	0.01
