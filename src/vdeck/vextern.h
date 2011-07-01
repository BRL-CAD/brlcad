/*                       V E X T E R N . H
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
 */
/** @file vdeck/vextern.h
 *	Author:		Gary Moss
 */

/* Special characters.							*/
#define	LF		"\n"
#define BLANKS	"                                                                          "

/* Command line options.						*/
#define DECK		'd'
#define ERASE		'e'
#define INSERT		'i'
#define LIST		'l'
#define MENU		'?'
#define NUMBER		'n'
#define QUIT		'q'
#define REMOVE		'r'
#define RETURN		'\0'
#define SHELL		'!'
#define SORT_TOC	's'
#define TOC		't'
#define UNKNOWN		default

/* Prompts.								*/
#define CMD_PROMPT	"\ncommand( ? for menu )>> "
#define LST_PROMPT	"\nSOLIDS LIST( ? for menu )> "
#define PROMPT		"vdeck> "

/* Size limits.								*/
#define MAXLN	80	/* max length of input line */

#define MAXARG	20	/* max arguments on command line */

/* Standard flag settings.						*/
#define YES	1
#define NO	0

extern int	debug;
extern char	*usage[], *cmd[];
extern mat_t	identity;

extern void		abort_sig(int sig), quit(int sig);
extern void		toc(void), list_toc(char **args);
extern void		prompt(char *fmt);

extern int	curr_ct;
extern char	*arg_list[];
extern int	arg_ct;
extern int	tmp_ct;

extern char	*objfile;

extern int	solfd;		extern char	*st_file;
extern int	regfd;		extern char	*rt_file;
extern int	ridfd;		extern char	*id_file;

extern int	ndir, nns, nnr;

extern int			delsol, delreg;
extern char			buff[];
extern long			savsol;

extern jmp_buf		env;
#define EPSILON		0.0001
#define CONV_EPSILON	0.01

extern struct db_i	*dbip;		/* Database instance ptr */

/*
 * Local Variables:
 * mode: C
 * tab-width: 8
 * indent-tabs-mode: t
 * c-file-style: "stroustrup"
 * End:
 * ex: shiftwidth=4 tabstop=8
 */
