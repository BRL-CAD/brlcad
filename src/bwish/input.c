/*                         I N P U T . C
 * BRL-CAD
 *
 * Copyright (c) 1998-2011 United States Government as represented by
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
 *
 */
/** @file bwish/input.c
 *
 * Input handling routines. Some of this code was borrowed from mged/ged.c
 * and modified for use in this application.
 *
 */

#include "common.h"

/* system headers */
#include <string.h>
#include <ctype.h>
#include <time.h>
#ifdef HAVE_SYS_TIME_H
#  include <sys/time.h>
#endif

#include "bio.h"

/* interface headers */
#include "tcl.h"
#include "bu.h"
#include "libtermio.h"


#define CTRL_A      1
#define CTRL_B      2
#define CTRL_D      4
#define CTRL_E      5
#define CTRL_F      6
#define CTRL_K      11
#define CTRL_L      12
#define CTRL_N      14
#define CTRL_P      16
#define CTRL_T      20
#define CTRL_U      21
#define CTRL_W      '\027'
#define ESC         27
#define BACKSPACE   '\b'
#define DELETE      127
#ifdef BWISH
#define PROMPT "\rbwish> "
#else
#define PROMPT "\rbtclsh> "
#endif

#define SPACES "                                                                                                                                                                                                                                                                                                           "

/* defined in tcl.c */
extern void Cad_Exit(int status);

/* defined in cmd.c */
extern struct bu_vls *history_prev(void);
extern struct bu_vls *history_next(void);
extern void history_record_priv(struct bu_vls *cmdp, struct timeval *start, struct timeval *finish, int status);

/* defined in main.c */
extern Tcl_Interp *INTERP;

HIDDEN void inputHandler(ClientData clientData, int mask);
HIDDEN void processChar(char ch);
HIDDEN void insert_prompt(void);
HIDDEN void insert_char(char ch);
HIDDEN void insert_beep(void);

static struct bu_vls input_str;
static struct bu_vls input_str_prefix;
static size_t input_str_index;
static struct bu_vls scratchline;
static struct bu_vls prompt;

void
initInput(void)
{
    bu_vls_init(&input_str);
    bu_vls_init(&input_str_prefix);
    bu_vls_init(&scratchline);
    bu_vls_init(&prompt);
    input_str_index = 0;

    Tcl_CreateFileHandler(STDIN_FILENO, TCL_READABLE,
			  inputHandler, (ClientData)STDIN_FILENO);

    bu_vls_strcpy(&prompt, PROMPT);
    save_Tty(fileno(stdin));
    set_Cbreak(fileno(stdin));
    clr_Echo(fileno(stdin));
    insert_prompt();
}


HIDDEN void
inputHandler(ClientData clientData, int UNUSED(mask))
{
    int count;
    char ch;
    long fd;
    char buf[4096];
    int i;

    fd = (long)clientData;

    count = read((int)fd, (void *)buf, 4096);

    if (count < 0) {
	perror("READ ERROR");
    }
    if (count <= 0 && feof(stdin)) {
	Cad_Exit(TCL_OK);
    }

    /* Process everything in buf */
    for (i = 0, ch = buf[i]; i < count; ch = buf[++i])
	processChar(ch);
}


/* Process character */
HIDDEN void
processChar(char ch)
{
    struct bu_vls *vp;
    struct bu_vls temp;
    static int escaped = 0;
    static int bracketed = 0;
    static int freshline = 1;

    /* ANSI arrow keys */
    if (escaped && bracketed) {
	if (ch == 'A') ch = CTRL_P;
	if (ch == 'B') ch = CTRL_N;
	if (ch == 'C') ch = CTRL_F;
	if (ch == 'D') ch = CTRL_B;
	escaped = bracketed = 0;
    }

    switch (ch) {
	case ESC:           /* Used for building up ANSI arrow keys */
	    escaped = 1;
	    break;
	case '\n':          /* Carriage return or line feed */
	case '\r':
	    bu_log("\n");   /* Display newline */

	    bu_vls_printf(&input_str_prefix, "%s%V\n",
			  bu_vls_strlen(&input_str_prefix) > 0 ? " " : "",
			  &input_str);

	    /* If this forms a complete command (as far as the Tcl parser is
	       concerned) then execute it. */
	    if (Tcl_CommandComplete(bu_vls_addr(&input_str_prefix))) {
		int status;
		struct timeval start, finish;
		const char *result;

		reset_Tty(fileno(stdin));
		gettimeofday(&start, (struct timezone *)NULL);
		status = Tcl_Eval(INTERP, bu_vls_addr(&input_str_prefix));
		gettimeofday(&finish, (struct timezone *)NULL);
		result = Tcl_GetStringResult(INTERP);
		if (strlen(result))
		    bu_log("%s\n", result);

		history_record_priv(&input_str_prefix, &start, &finish, status);
		bu_vls_trunc(&input_str_prefix, 0);
		bu_vls_trunc(&input_str, 0);
		set_Cbreak(fileno(stdin)); /* Back to single-character mode */
		clr_Echo(fileno(stdin));

		/* reset prompt */
		bu_vls_strcpy(&prompt, PROMPT);

	    } else {
		bu_vls_trunc(&input_str, 0);
		bu_vls_strcpy(&prompt, "\r? ");
	    }
	    insert_prompt();
	    input_str_index = 0;
	    freshline = 1;
	    escaped = bracketed = 0;
	    break;
	case BACKSPACE:
	case DELETE:
	    if (input_str_index == 0) {
		insert_beep();
		break;
	    }

	    if (input_str_index == bu_vls_strlen(&input_str)) {
		bu_log("\b \b");
		bu_vls_trunc(&input_str, bu_vls_strlen(&input_str)-1);
	    } else {
		bu_vls_init(&temp);
		bu_vls_strcat(&temp, bu_vls_addr(&input_str)+input_str_index);
		bu_vls_trunc(&input_str, input_str_index-1);
		bu_log("\b%V ", &temp);
		insert_prompt();
		bu_log("%V", &input_str);
		bu_vls_vlscat(&input_str, &temp);
		bu_vls_free(&temp);
	    }
	    --input_str_index;
	    escaped = bracketed = 0;
	    break;
	case CTRL_A:                    /* Go to beginning of line */
	    insert_prompt();
	    input_str_index = 0;
	    escaped = bracketed = 0;
	    break;
	case CTRL_E:                    /* Go to end of line */
	    if (input_str_index < bu_vls_strlen(&input_str)) {
		bu_log("%s", bu_vls_addr(&input_str)+input_str_index);
		input_str_index = bu_vls_strlen(&input_str);
	    }
	    escaped = bracketed = 0;
	    break;
	case CTRL_D:                    /* Delete character at cursor */
	    if (input_str_index == bu_vls_strlen(&input_str)) {
		insert_beep(); /* Beep if at end of input string */
		break;
	    }
	    bu_vls_init(&temp);
	    bu_vls_strcat(&temp, bu_vls_addr(&input_str)+input_str_index+1);
	    bu_vls_trunc(&input_str, input_str_index);
	    bu_log("%V ", &temp);
	    insert_prompt();
	    bu_log("%V", &input_str);
	    bu_vls_vlscat(&input_str, &temp);
	    bu_vls_free(&temp);
	    escaped = bracketed = 0;
	    break;
	case CTRL_U:                   /* Delete whole line */
	    insert_prompt();
	    bu_log("%*s", bu_vls_strlen(&input_str), SPACES);
	    insert_prompt();
	    bu_vls_trunc(&input_str, 0);
	    input_str_index = 0;
	    escaped = bracketed = 0;
	    break;
	case CTRL_K:                    /* Delete to end of line */
	    bu_log("%*s", bu_vls_strlen(&input_str)-input_str_index, SPACES);
	    bu_vls_trunc(&input_str, input_str_index);
	    insert_prompt();
	    bu_log("%V", &input_str);
	    escaped = bracketed = 0;
	    break;
	case CTRL_L:                   /* Redraw line */
	    bu_log("\n");
	    insert_prompt();
	    bu_log("%V", &input_str);
	    if (input_str_index == bu_vls_strlen(&input_str))
		break;
	    insert_prompt();
	    bu_log("%*V", input_str_index, &input_str);
	    escaped = bracketed = 0;
	    break;
	case CTRL_B:                   /* Back one character */
	    if (input_str_index == 0) {
		insert_beep();
		break;
	    }
	    --input_str_index;
	    bu_log("\b"); /* hopefully non-destructive! */
	    escaped = bracketed = 0;
	    break;
	case CTRL_F:                   /* Forward one character */
	    if (input_str_index == bu_vls_strlen(&input_str)) {
		insert_beep();
		break;
	    }

	    bu_log("%c", bu_vls_addr(&input_str)[input_str_index]);
	    ++input_str_index;
	    escaped = bracketed = 0;
	    break;
	case CTRL_T:                  /* Transpose characters */
	    if (input_str_index == 0) {
		insert_beep();
		break;
	    }
	    if (input_str_index == bu_vls_strlen(&input_str)) {
		bu_log("\b");
		--input_str_index;
	    }
	    ch = bu_vls_addr(&input_str)[input_str_index];
	    bu_vls_addr(&input_str)[input_str_index] =
		bu_vls_addr(&input_str)[input_str_index - 1];
	    bu_vls_addr(&input_str)[input_str_index - 1] = ch;
	    bu_log("\b%*s", 2, bu_vls_addr(&input_str)+input_str_index-1);
	    ++input_str_index;
	    escaped = bracketed = 0;
	    break;
	case CTRL_N:                  /* Next history command */
	case CTRL_P:                  /* Last history command */
#if 1
	    /* Work the history routines to get the right string */
	    if (freshline) {
		if (ch == CTRL_P) {
		    vp = history_prev();
		    if (vp == NULL) {
			insert_beep();
			break;
		    }
		    bu_vls_trunc(&scratchline, 0);
		    bu_vls_vlscat(&scratchline, &input_str);
		    freshline = 0;
		} else {
		    insert_beep();
		    break;
		}
	    } else {
		if (ch == CTRL_P) {
		    vp = history_prev();
		    if (vp == NULL) {
			insert_beep();
			break;
		    }
		} else {
		    vp = history_next();
		    if (vp == NULL) {
			vp = &scratchline;
			freshline = 1;
		    }
		}
	    }
	    insert_prompt();
	    bu_log("%*s", bu_vls_strlen(&input_str), SPACES);
	    insert_prompt();
	    bu_vls_trunc(&input_str, 0);
	    bu_vls_vlscat(&input_str, vp);
	    if (bu_vls_addr(&input_str)[bu_vls_strlen(&input_str)-1] == '\n')
		bu_vls_trunc(&input_str, bu_vls_strlen(&input_str)-1); /* del \n */
	    bu_log("%V", &input_str);
	    input_str_index = bu_vls_strlen(&input_str);
	    escaped = bracketed = 0;
#endif
	    break;
	case CTRL_W:                   /* backward-delete-word */
	    {
		char *start;
		char *curr;
		int len;

		start = bu_vls_addr(&input_str);
		curr = start + input_str_index - 1;

		/* skip spaces */
		while (curr > start && *curr == ' ')
		    --curr;

		/* find next space */
		while (curr > start && *curr != ' ')
		    --curr;

		bu_vls_init(&temp);
		bu_vls_strcat(&temp, start+input_str_index);

		if (curr == start)
		    input_str_index = 0;
		else
		    input_str_index = curr - start + 1;

		len = bu_vls_strlen(&input_str);
		bu_vls_trunc(&input_str, input_str_index);
		insert_prompt();
		bu_log("%V%V%*s", &input_str, &temp, len - input_str_index, SPACES);
		insert_prompt();
		bu_log("%V", &input_str);
		bu_vls_vlscat(&input_str, &temp);
		bu_vls_free(&temp);
	    }

	    escaped = bracketed = 0;
	    break;
	case 'd':
	    if (escaped) {
		/* delete-word */
		char *start;
		char *curr;
		int i;

		start = bu_vls_addr(&input_str);
		curr = start + input_str_index;

		/* skip spaces */
		while (*curr != '\0' && *curr == ' ')
		    ++curr;

		/* find next space */
		while (*curr != '\0' && *curr != ' ')
		    ++curr;

		i = curr - start;
		bu_vls_init(&temp);
		bu_vls_strcat(&temp, curr);
		bu_vls_trunc(&input_str, input_str_index);
		insert_prompt();
		bu_log("%V%V%*s", &input_str, &temp, i - input_str_index, SPACES);
		insert_prompt();
		bu_log("%V", &input_str);
		bu_vls_vlscat(&input_str, &temp);
		bu_vls_free(&temp);
	    } else
		insert_char(ch);

	    escaped = bracketed = 0;
	    break;
	case 'f':
	    if (escaped) {
		/* forward-word */
		char *start;
		char *curr;

		start = bu_vls_addr(&input_str);
		curr = start + input_str_index;

		/* skip spaces */
		while (*curr != '\0' && *curr == ' ')
		    ++curr;

		/* find next space */
		while (*curr != '\0' && *curr != ' ')
		    ++curr;

		input_str_index = curr - start;
		bu_vls_init(&temp);
		bu_vls_strcat(&temp, start+input_str_index);
		bu_vls_trunc(&input_str, input_str_index);
		insert_prompt();
		bu_log("%V", &input_str);
		bu_vls_vlscat(&input_str, &temp);
		bu_vls_free(&temp);
	    } else
		insert_char(ch);

	    escaped = bracketed = 0;
	    break;
	case 'b':
	    if (escaped) {
		/* backward-word */
		char *start;
		char *curr;

		start = bu_vls_addr(&input_str);
		curr = start + input_str_index - 1;

		/* skip spaces */
		while (curr > start && *curr == ' ')
		    --curr;

		/* find next space */
		while (curr > start && *curr != ' ')
		    --curr;

		if (curr == start)
		    input_str_index = 0;
		else
		    input_str_index = curr - start + 1;

		bu_vls_init(&temp);
		bu_vls_strcat(&temp, start+input_str_index);
		bu_vls_trunc(&input_str, input_str_index);
		insert_prompt();
		bu_log("%V", &input_str);
		bu_vls_vlscat(&input_str, &temp);
		bu_vls_free(&temp);
	    } else
		insert_char(ch);

	    escaped = bracketed = 0;
	    break;
	case '[':
	    if (escaped) {
		bracketed = 1;
		break;
	    }
	    /* Fall through if not escaped! */
	default:
	    if (!isprint(ch))
		break;

	    insert_char(ch);
	    escaped = bracketed = 0;
	    break;
    }
}


HIDDEN void
insert_prompt(void)
{
    bu_log("%V", &prompt);
}


HIDDEN void
insert_char(char ch)
{
    if (input_str_index == bu_vls_strlen(&input_str)) {
	bu_log("%c", (int)ch);
	bu_vls_putc(&input_str, (int)ch);
	++input_str_index;
    } else {
	struct bu_vls temp;

	bu_vls_init(&temp);
	bu_vls_strcat(&temp, bu_vls_addr(&input_str)+input_str_index);
	bu_vls_trunc(&input_str, input_str_index);
	bu_log("%c%V", (int)ch, &temp);
	insert_prompt();
	bu_vls_putc(&input_str, (int)ch);
	bu_log("%V", &input_str);
	bu_vls_vlscat(&input_str, &temp);
	++input_str_index;
	bu_vls_free(&temp);
    }
}


HIDDEN void
insert_beep(void)
{
    bu_log("%c", 7);
}


/*
 * Local Variables:
 * mode: C
 * tab-width: 8
 * indent-tabs-mode: t
 * c-file-style: "stroustrup"
 * End:
 * ex: shiftwidth=4 tabstop=8
 */
