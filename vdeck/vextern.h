/*
 *	@(#) vextern.h			retrieved: 8/13/86 at 08:19:03,
 *	@(#) version 1.6		last edit: 10/11/83 at 10:57:41.
 *
 *	Written by Gary S. Moss.
 *	All rights reserved, Ballistic Research Laboratory.
 */
#include <math.h>
#include "./ged_types.h"
#include "./3d.h"
#include "./vdeck.h"

extern int	debug;
extern char	*usage[], *cmd[];
extern mat_t	xform, notrans, identity;

extern Directory	directory[], *dp;
extern Record record;
extern struct findrr findrr[];

extern char	dir_names[], *dir_last;

extern int	discr[];

extern long		lseek();
extern int		abort(), quit();
extern Directory	*lookup(), *diradd();

extern char	*toc_list[];
extern int	toc_ct;
extern char	*curr_list[];
extern int	curr_ct;
extern char	*arg_list[];
extern int	arg_ct, max_args;
extern char	*tmp_list[];
extern int	tmp_ct;

extern int	objfd;		extern char	*objfile;
extern int	solfd;		extern char	st_file[];
extern int	regfd;		extern char	rt_file[];
extern int	ridfd;		extern char	id_file[];
extern int	idfd, rd_idfd;	extern char	disc_file[];
extern int	rrfd, rd_rrfd;	extern char	reg_file[];

extern int	ndir, nns, nnr, numrr;

extern int		regflag, orflag, item, space, delsol, delreg;
extern Directory	*path[];
extern int		isave;
extern char		buff[],	name[];
extern char		operate;
extern long		savsol;
extern struct		ident ident, idbuf;

#include <setjmp.h>
extern jmp_buf		env;

