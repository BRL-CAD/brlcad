/*
 *	d e c k _ p r o c . h
 *	Externals defined in deck_glob.h
 */

extern mat_t	xform, notrans, identity;

extern Directory	directory[], *dp, *lookup();
extern Record record;
extern struct findrr findrr[];

extern char	dir_names[], *dir_last;

extern int	discr[];

extern long	lseek();
extern int	abort(), quit();
extern double	atof();

extern char	*toc_list[];
extern int	toc_ct;
extern char	*curr_list[];
extern int	curr_ct;
extern char	*arg_list[];
extern int	arg_ct, max_args;
extern int	*tmp_list[];
extern int	tmp_ct;

extern int	objfd;		extern char	*objfile;
extern int	solfd;		extern char	st_file[];
extern int	regfd;		extern char	rt_file[];
extern int	ridfd;		extern char	id_file[];
extern int	idfd, rd_idfd;	extern char	disc_file[];
extern int	rrfd, rd_rrfd;	extern char	reg_file[];

extern int	ndir, nns, nnr, numrr;

extern int	regflag, orflag, item, space, delsol, delreg;
extern int	path[];
extern int	isave;
extern char	buff[],	name[], filter[], *term;
extern char	operate;
extern long	savsol;
extern jmp_buf	env;

