/*                       C M D H I S T . C
 * BRL-CAD
 *
 * Copyright (C) 1998-2005 United States Government as represented by
 * the U.S. Army Research Laboratory.
 *
 * This library is free software; you can redistribute it and/or
 * modify it under the terms of the GNU Lesser General Public License
 * as published by the Free Software Foundation; either version 2 of
 * the License, or (at your option) any later version.
 *
 * This library is distributed in the hope that it will be useful, but
 * WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
 * Library General Public License for more details.
 *
 * You should have received a copy of the GNU Lesser General Public
 * License along with this file; see the file named COPYING for more
 * information.
 */

/** \addtogroup cmd */
/*@{*/
/** @file cmdhist.c
 * The history routines were borrowed from mged/history.c
 * and modified to work with command history objects.
 *
 *  Author -
 *	   Robert G. Parker
 *
 *  Authors of mged/history.c -
 *	   Glenn Durfee
 *	   Robert G. Parker
 *
 *  Source -
 *	The U. S. Army Research Laboratory
 *	Aberdeen Proving Ground, Maryland  21005-5068  USA
 *  
 */
/*@}*/

#include "common.h"


#ifdef HAVE_STRING_H
#include <string.h>
#endif

#include "tcl.h"

#include "machine.h"
#include "cmd.h"

/*
 *	H I S T O R Y _ R E C O R D
 *
 *	Stores the given command with start and finish times in the
 *	history vls'es.
 */
static void
history_record(struct bu_cmdhist_obj *chop, struct bu_vls *cmdp, struct timeval *start, struct timeval *finish, int status)
                                 
                         
                                    
                   /* Either TCL_OK or TCL_ERROR */
{
	struct bu_cmdhist *new_hist;

	if (strcmp(bu_vls_addr(cmdp), "\n") == 0)
		return;

	new_hist = (struct bu_cmdhist *)bu_malloc(sizeof(struct bu_cmdhist),
						  "mged history");
	bu_vls_init(&new_hist->h_command);
	bu_vls_vlscat(&new_hist->h_command, cmdp);
	new_hist->h_start = *start;
	new_hist->h_finish = *finish;
	new_hist->h_status = status;
	BU_LIST_INSERT(&chop->cho_head.l, &new_hist->l);

	chop->cho_curr = &chop->cho_head;
}

static int
timediff(struct timeval *tvdiff, struct timeval *start, struct timeval *finish)
{
	if (finish->tv_sec == 0 && finish->tv_usec == 0)
		return -1;
	if (start->tv_sec == 0 && start->tv_usec == 0)
		return -1;
    
	tvdiff->tv_sec = finish->tv_sec - start->tv_sec;
	tvdiff->tv_usec = finish->tv_usec - start->tv_usec;
	if (tvdiff->tv_usec < 0) {
		--tvdiff->tv_sec;
		tvdiff->tv_usec += 1000000L;
	}

	return 0;
}

/*
 * Prints out the command history.
 *
 * USAGE:
 *        procname history [-delays] [-outfile filename]
 */
int
bu_cmdhist_history(ClientData clientData, Tcl_Interp *interp, int argc, char **argv)
{
	struct bu_cmdhist_obj *chop = (struct  bu_cmdhist_obj *)clientData;
	FILE *fp;
	int with_delays = 0;
	struct bu_cmdhist *hp, *hp_prev;
	struct bu_vls str;
	struct timeval tvdiff;

	if (argc < 2 || 5 < argc) {
		struct bu_vls vls;

		bu_vls_init(&vls);
		bu_vls_printf(&vls, "help history");
		Tcl_Eval(interp, bu_vls_addr(&vls));
		bu_vls_free(&vls);
		return TCL_ERROR;
	}

	fp = NULL;
	while (argc >= 3)  {
		if (strcmp(argv[2], "-delays") == 0)
			with_delays = 1;
		else if( strcmp(argv[2], "-outfile") == 0 ) {
			if (fp != NULL) {
				fclose(fp);
				Tcl_AppendResult(interp, "history: -outfile option given more than once\n",
						 (char *)NULL);
				return TCL_ERROR;
			} else if (argc < 4 || strcmp(argv[3], "-delays") == 0) {
				Tcl_AppendResult(interp, "history: I need a file name\n", (char *)NULL);
				return TCL_ERROR;
			} else {
				fp = fopen( argv[3], "a+" );
				if (fp == NULL) {
					Tcl_AppendResult(interp, "history: error opening file", (char *)NULL);
					return TCL_ERROR;
				}
				--argc;
				++argv;
			}
		} else {
			Tcl_AppendResult(interp, "Invalid option ", argv[2], "\n", (char *)NULL);
		}
		--argc;
		++argv;
	}

	bu_vls_init(&str);
	for (BU_LIST_FOR(hp, bu_cmdhist, &chop->cho_head.l)) {
		bu_vls_trunc(&str, 0);
		hp_prev = BU_LIST_PREV(bu_cmdhist, &hp->l);
		if (with_delays && BU_LIST_NOT_HEAD(hp_prev, &chop->cho_head.l)) {
			if (timediff(&tvdiff, &(hp_prev->h_finish), &(hp->h_start)) >= 0)
				bu_vls_printf(&str, "delay %d %d\n", tvdiff.tv_sec,
					      tvdiff.tv_usec);

		}

		if (hp->h_status == TCL_ERROR)
			bu_vls_printf(&str, "# ");
		bu_vls_vlscat(&str, &(hp->h_command));

		if (fp != NULL)
			bu_vls_fwrite(fp, &str);
		else
			Tcl_AppendResult(interp, bu_vls_addr(&str), (char *)NULL);
	}

	if (fp != NULL)
		fclose(fp);

	return TCL_OK;
}

/*
 * Add a command to the history list.
 *
 * USAGE:
 *        procname add cmd
 */
int
bu_cmdhist_add(ClientData clientData, Tcl_Interp *interp, int argc, char **argv)
{
	struct bu_cmdhist_obj *chop = (struct  bu_cmdhist_obj *)clientData;
	struct bu_vls vls;
	struct timeval zero;

	if(argc != 3){
		bu_vls_init(&vls);
		bu_vls_printf(&vls, "helplib cmdhist_add");
		Tcl_Eval(interp, bu_vls_addr(&vls));
		bu_vls_free(&vls);
		return TCL_ERROR;
	}

	if (argv[2][0] == '\n' || argv[2][0] == '\0')
		return TCL_OK;

	bu_vls_init(&vls);
	bu_vls_strcpy(&vls, argv[2]);
	if (argv[2][strlen(argv[2])-1] != '\n')
		bu_vls_putc(&vls, '\n');

	zero.tv_sec = zero.tv_usec = 0L;
	history_record(chop, &vls, &zero, &zero, TCL_OK);

	bu_vls_free(&vls);
	return TCL_OK;
}

/*
 * Set the current command to the previous command.
 *
 * USAGE:
 *        procname prev
 */
int
bu_cmdhist_prev(ClientData clientData, Tcl_Interp *interp, int argc, char **argv)
{
	struct bu_cmdhist_obj *chop = (struct  bu_cmdhist_obj *)clientData;
	struct bu_cmdhist *hp;

	if (argc != 2) {
		struct bu_vls vls;

		bu_vls_init(&vls);
		bu_vls_printf(&vls, "helplib cmdhist_prev");
		Tcl_Eval(interp, bu_vls_addr(&vls));
		bu_vls_free(&vls);
		return TCL_ERROR;
	}

	hp = BU_LIST_PLAST(bu_cmdhist, chop->cho_curr);
	if (BU_LIST_NOT_HEAD(hp, &chop->cho_head.l))
		chop->cho_curr = hp;

	Tcl_AppendResult(interp, bu_vls_addr(&chop->cho_curr->h_command), (char *)NULL);
	return TCL_OK;
}

/*
 * Return the current command.
 *
 * USAGE:
 *        procname curr
 */
int
bu_cmdhist_curr(ClientData clientData, Tcl_Interp *interp, int argc, char **argv)
{
	struct bu_cmdhist_obj *chop = (struct  bu_cmdhist_obj *)clientData;

	if (argc != 2) {
		struct bu_vls vls;

		bu_vls_init(&vls);
		bu_vls_printf(&vls, "helplib cmdhist_curr");
		Tcl_Eval(interp, bu_vls_addr(&vls));
		bu_vls_free(&vls);
		return TCL_ERROR;
	}

	if (BU_LIST_NOT_HEAD(chop->cho_curr, &chop->cho_head.l))
		Tcl_AppendResult(interp, bu_vls_addr(&chop->cho_curr->h_command), (char *)NULL);

	return TCL_OK;
}

/*
 * Set the current command to the next command.
 *
 * USAGE:
 *        procname next
 */
int
bu_cmdhist_next(ClientData clientData, Tcl_Interp *interp, int argc, char **argv)
{
	struct bu_cmdhist_obj *chop = (struct  bu_cmdhist_obj *)clientData;

	if (argc != 2) {
		struct bu_vls vls;

		bu_vls_init(&vls);
		bu_vls_printf(&vls, "helplib cmdhist_next");
		Tcl_Eval(interp, bu_vls_addr(&vls));
		bu_vls_free(&vls);
		return TCL_ERROR;
	}

	if (BU_LIST_IS_HEAD(chop->cho_curr, &chop->cho_head.l))
		return TCL_ERROR;
    
	chop->cho_curr = BU_LIST_PNEXT(bu_cmdhist, chop->cho_curr);
	if (BU_LIST_IS_HEAD(chop->cho_curr, &chop->cho_head.l))
		return TCL_ERROR;

	Tcl_AppendResult(interp, bu_vls_addr(&chop->cho_curr->h_command), (char *)NULL);
	return TCL_OK;
}

#if 0
/*
 *	F _ D E L A Y
 *
 * 	Uses select to delay for the specified amount of seconds and 
 *	  microseconds.
 */

int
f_delay(clientData, interp, argc, argv)
     ClientData clientData;
     Tcl_Interp *interp;
     int argc;
     char **argv;
{
	struct timeval tv;

	if(argc < 3 || 3 < argc){
		struct bu_vls vls;

		bu_vls_init(&vls);
		bu_vls_printf(&vls, "help delay");
		Tcl_Eval(interp, bu_vls_addr(&vls));
		bu_vls_free(&vls);
		return TCL_ERROR;
	}

	tv.tv_sec = atoi(argv[1]);
	tv.tv_usec = atoi(argv[2]);
	select(0, NULL, NULL, NULL, &tv);

	return TCL_OK;
}
#endif

/*
 * Local Variables:
 * mode: C
 * tab-width: 8
 * c-basic-offset: 4
 * indent-tabs-mode: t
 * End:
 * ex: shiftwidth=4 tabstop=8
 */
