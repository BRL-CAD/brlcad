/*                    U T I L . C P P
 * BRL-CAD
 *
 * Copyright (c) 2017 United States Government as represented by
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
/** @file util.cpp
 *
 */

#include "common.h"
#include "creo-brl.h"

int creo_logging_opts(void *data, void *vstr){
    struct creo_conv_info *cinfo = (struct creo_conv_info *)data;
    const char *str = (const char *)vstr;
    if (cinfo->logger_type == LOGGER_TYPE_NONE) return 0;
    switch (cinfo->logger_type) {
	case LOGGER_TYPE_FAILURE:
	    if (cinfo->curr_msg_type == MSG_FAIL) {
		if (cinfo->logger) {
		    fprintf(cinfo->logger, "FAILURE: %s", str);
		}
		if (cinfo->print_to_console) {
		    if (LIKELY(stderr != NULL)) {
			fprintf(stderr, "FAILURE: %s", str);
			fflush(stderr);
		    }
		}
	    }
	    break;
	case LOGGER_TYPE_SUCCESS:
	    if (cinfo->curr_msg_type == MSG_OK) {
		if (cinfo->logger) {
		    fprintf(cinfo->logger, "SUCCESS: %s", str);
		}
		if (cinfo->print_to_console) {
		    if (LIKELY(stderr != NULL)) {
			fprintf(stderr, "SUCCESS: %s", str);
			fflush(stderr);
		    }
		}
	    }
	    break;
	case LOGGER_TYPE_FAILURE_OR_SUCCESS:
	    if (cinfo->curr_msg_type < MSG_DEBUG) {
		if (cinfo->curr_msg_type == MSG_FAIL) {
		    if (cinfo->logger) {
			fprintf(cinfo->logger, "FAILURE: %s", str);
		    }
		    if (cinfo->print_to_console) {
			if (LIKELY(stderr != NULL)) {
			    fprintf(stderr, "FAILURE: %s", str);
			    fflush(stderr);
			}
		    }
		}
		if (cinfo->curr_msg_type == MSG_OK) {
		    if (cinfo->logger) {
			fprintf(cinfo->logger, "SUCCESS: %s", str);
		    }
		    if (cinfo->print_to_console) {
			if (LIKELY(stderr != NULL)) {
			    fprintf(stderr, "SUCCESS: %s", str);
			    fflush(stderr);
			}
		    }
		}
	    }
	    break;
	default:
	    if (cinfo->curr_msg_type == MSG_FAIL) {
		if (cinfo->logger) {
		    fprintf(cinfo->logger, "FAILURE: %s", str);
		}
		if (cinfo->print_to_console) {
		    if (cinfo->print_to_console) {
			if (LIKELY(stderr != NULL)) {
			    fprintf(stderr, "FAILURE: %s", str);
			    fflush(stderr);
			}
		    }
		}
	    }
	    if (cinfo->curr_msg_type == MSG_OK) {
		if (cinfo->logger) {
		    fprintf(cinfo->logger, "SUCCESS: %s", str);
		}
		if (cinfo->print_to_console) {
		    if (LIKELY(stderr != NULL)) {
			fprintf(stderr, "SUCCESS: %s", str);
			fflush(stderr);
		    }
		}
	    }
	    if (cinfo->curr_msg_type == MSG_DEBUG) {
		if (cinfo->logger) {
		    fprintf(cinfo->logger, "DEBUG: %s", str);
		}
		if (cinfo->print_to_console) {
		    if (LIKELY(stderr != NULL)) {
			fprintf(stderr, "DEBUG: %s", str);
			fflush(stderr);
		    }
		}
	    }
	    break;
    }
}

extern "C" ProError
creo_log(struct creo_conv_info *cinfo, int msg_type, ProError status, const char *fmt, ...) {
    /* TODO - need creo specific semaphore lock for this if it's going to be used
     * in multi-threading situations... - probably can't use libbu's logging safely */

    /* Can't do nested variable argument functions, so printf the message here */
    va_list ap;
    char msg[100000];
    va_start(ap, fmt);
    vsprintf(msg, fmt, ap);
    va_end(ap);

    /* Set the type and hooks, then call libbu */
    cinfo->curr_msg_type = msg_type;
    bu_log_add_hook(creo_logging_opts, (void *)cinfo);
    bu_log("%s", msg);
    bu_log_delete_hook(creo_logging_opts, (void *)cinfo);

    return status;
}


extern "C" long int
wstr_to_long(struct creo_conv_info *cinfo, wchar_t *tmp_str)
{
    long int ret;
    char *endptr = NULL;
    ProCharLine astr;
    ProWstringToString(astr, *tmp_str);
    errno = 0;
    ret = strtol(astr, &endptr, 0);
    if (endptr != NULL && strlen(endptr) > 0) {
        /* Had some invalid character in the input, fail */
        creo_log(cinfo, MSG_FAIL, PRO_TK_NO_ERROR, "Invalid string specifier for int: %s\n", astr);
        return -1;
    }
    return ret;
}

extern "C" double
wstr_to_double(struct creo_conv_info *cinfo, wchar_t *tmp_str)
{
    double ret;
    char *endptr = NULL;
    ProCharLine astr;
    ProWstringToString(astr, *tmp_str);
    errno = 0;
    ret = strtod(astr, &endptr);
    if (endptr != NULL && strlen(endptr) > 0) {
        /* Had some invalid character in the input, fail */
        creo_log(cinfo, MSG_FAIL, PRO_TK_NO_ERROR, "Invalid string specifier for double: %s\n", astr);
        return -1.0;
    }
    return ret;
}

extern "C" void
kill_error_dialog( char *dialog, char *component, ProAppData appdata )
{
    (void)ProUIDialogDestroy( "creo_brl_error" );
}

extern "C" void
kill_gen_error_dialog( char *dialog, char *component, ProAppData appdata )
{
    (void)ProUIDialogDestroy( "creo_brl_gen_error" );
}


extern "C" void
lower_case( char *name )
{
    unsigned char *c;

    c = (unsigned char *)name;
    while ( *c ) {
	(*c) = tolower( *c );
	c++;
    }
}

extern "C" void
make_legal( char *name )
{
    unsigned char *c;

    c = (unsigned char *)name;
    while ( *c ) {
	if ( *c <= ' ' || *c == '/' || *c == '[' || *c == ']' ) {
	    *c = '_';
	} else if ( *c > '~' ) {
	    *c = '_';
	}
	c++;
    }
}


/* TODO - investigate ProParameter as a source of name generation for
 * CREO parts.  Currently being done by a text processing script manually. */
extern "C" char *
creo_param_name(struct creo_conv_info *cinfo, wchar_t *creo_name, ProType type)
{
    struct bu_vls *tmp_name;
    std::set<struct bu_vls *, StrCmp>::iterator found;
}


/* create a unique BRL-CAD object name from a possibly illegal name */
extern "C" char *
create_unique_name(struct creo_conv_info *cinfo, char *name )
{
    struct bu_vls *tmp_name;
    std::pair<std::set<struct bu_vls *, StrCmp>::iterator, bool> ret;
    long count=0;

    if ( cinfo->logger_type == LOGGER_TYPE_ALL ) {
	fprintf( cinfo->logger, "create_unique_name( %s )\n", name );
    }

    /* create a unique name */
    BU_GET(tmp_name, struct bu_vls);
    bu_vls_init(tmp_name);
    bu_vls_strcpy(tmp_name, name);
    lower_case(bu_vls_addr(tmp_name));
    make_legal(bu_vls_addr(tmp_name));
    ret = cinfo->brlcad_names->insert(tmp_name);
    while (ret.second == false) {
	(void)bu_namegen(tmp_name, NULL, NULL);
	count++;
	if ( cinfo->logger_type == LOGGER_TYPE_ALL ) {
	    fprintf( cinfo->logger, "\tTrying %s\n", bu_vls_addr(tmp_name) );
	}
	if (count == LONG_MAX) {
	    bu_vls_free(tmp_name);
	    BU_PUT(tmp_name, struct bu_vls);
	    return NULL;
	}
	ret = cinfo->brlcad_names->insert(tmp_name);
    }

    if ( cinfo->logger_type == LOGGER_TYPE_ALL ) {
	fprintf( cinfo->logger, "\tnew name for %s is %s\n", name, bu_vls_addr(tmp_name) );
    }
    return bu_vls_addr(tmp_name);
}

extern "C" char *
get_brlcad_name(struct creo_conv_info *cinfo, char *part_name )
{
    char *brlcad_name=NULL;
    struct bu_hash_entry *entry=NULL, *prev=NULL;
    int new_entry=0;
    unsigned long index=0;
    char *name_copy;

    name_copy = bu_strdup( part_name );
    lower_case( name_copy );

    if ( cinfo->logger_type == LOGGER_TYPE_ALL ) {
	fprintf( cinfo->logger, "get_brlcad_name( %s )\n", name_copy );
    }

    /* find name for this part in hash table */
    entry = bu_hash_tbl_find( cinfo->name_hash, (unsigned char *)name_copy, strlen( name_copy ), &prev, &index );

    if ( entry ) {
	if ( cinfo->logger_type == LOGGER_TYPE_ALL ) {
	    fprintf( cinfo->logger, "\treturning %s\n", (char *)bu_get_hash_value( entry ) );
	}
	bu_free( name_copy, "name_copy" );
	return (char *)bu_get_hash_value( entry );
    } else {

	/* must create a new name */
	brlcad_name = create_unique_name(cinfo, name_copy );
	entry = bu_hash_tbl_add( cinfo->name_hash, (unsigned char *)name_copy, strlen( name_copy ), &new_entry );
	bu_set_hash_value( entry, (unsigned char *)brlcad_name );
	if ( cinfo->logger_type == LOGGER_TYPE_ALL ) {
	    fprintf( cinfo->logger, "\tCreating new brlcad name (%s) for part (%s)\n", brlcad_name, name_copy );
	}
	bu_free( name_copy, "name_copy" );
	return brlcad_name;
    }
}

extern "C" int 
create_name_hash(struct creo_conv_info *cinfo, FILE *name_fd )
{
    char line[MAX_LINE_LEN];
    long line_no=0;

    while (bu_fgets(line, MAX_LINE_LEN, name_fd)) {
	char *part_no, *part_name, *ptr;
	line_no++;

	creo_log(cinfo, MSG_DEBUG, PRO_TK_NO_ERROR, "line %ld: %s", line_no, line);

	ptr = strtok( line, " \t\n" );
	if (!ptr) {
	    creo_log(cinfo, MSG_FAIL, PRO_TK_NO_ERROR, "Warning: unrecognizable line in part name file (bad part number):\n\t%s\n\tIgnoring\n", line );
	    continue;
	}
	part_no = bu_strdup(ptr);
	lower_case( part_no );

	/* match up to the EOL, everything up to it minus surrounding ws is the name */
	ptr = strtok( (char *)NULL, "\n" );
	if ( !ptr ) {
	    creo_log(cinfo, MSG_FAIL, PRO_TK_NO_ERROR, "Warning: unrecognizable line in part name file (bad part number):\n\t%s\n\tIgnoring\n", line );
	    continue;
	}
	if (cinfo->name_hash->find(part_no) != cinfo->name_hash->end()) {
	    creo_log(cinfo, MSG_DEBUG, PRO_TK_NO_ERROR, "\t\t\tHash table entry already exists for part number (%s)\n", part_no);
	    bu_free( part_no, "part_no" );
	    continue;
	}

	/* trim any left whitespace */
	while (isspace(*ptr) && (*ptr != '\n')) {ptr++;}
	part_name = ptr;

	/* trim any right whitespace */
	if (strlen(ptr) > 0) {
	    ptr += strlen(ptr) - 1;
	    while ((ptr != part_name) && (isspace(*ptr))) {
		*ptr = '\0';
		ptr--;
	    }
	}

	/* generate the name sans spaces, lowercase */
	lower_case( part_name );
	part_name = create_unique_name(cinfo, part_name );

	if ( cinfo->logger_type == LOGGER_TYPE_ALL ) {
	    fprintf( cinfo->logger, "\t\tpart_no = %s, part name = %s\n", part_no, part_name );
	}


	if ( cinfo->logger_type == LOGGER_TYPE_ALL ) {
	    fprintf( cinfo->logger, "\t\t\tCreating new hash tabel entry for above names\n" );
	}
	bu_hash_set( htbl, (unsigned char *)part_no, strlen( part_no ), part_name );
    }

    return htbl;
}

/* Test function */
ProError ShowMsg()
{
    ProUIMessageButton* button;
    ProUIMessageButton bresult;
    ProArrayAlloc(1, sizeof(ProUIMessageButton), 1, (ProArray*)&button);
    button[0] = PRO_UI_MESSAGE_OK;
    ProUIMessageDialogDisplay(PROUIMESSAGE_INFO, L"Hello World", L"Hello world!", button, PRO_UI_MESSAGE_OK, &bresult);
    ProArrayFree((ProArray*)&button);
    return PRO_TK_NO_ERROR;
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
