#ifndef DICTIONARY_H
#define DICTIONARY_H

/** **********************************************************************
** Module:  Dictionary \file dict.h
** Description: This module implements the dictionary abstraction.  A
**  dictionary is a repository for a number of objects, all of which
**  can be named using the same function.  A dictionary is limited to
**  storing items which can be cast to Generic (void*); in addition,
**  the constant NULL is used to report errors, so this is one value
**  which it is probably not a good idea to store in a dictionary.
** Constants:
**  DICTIONARY_NULL - the null dictionary
**
************************************************************************/

/*
 * This work was supported by the United States Government, and is
 * not subject to copyright.
 *
 * $Log: dict.h,v $
 * Revision 1.4  1997/01/21 19:17:11  dar
 * made C++ compatible
 *
 * Revision 1.3  1994/11/10  19:20:03  clark
 * Update to IS
 *
 * Revision 1.2  1993/10/15  18:49:23  libes
 * CADDETC certified
 *
 * Revision 1.5  1993/01/19  22:45:07  libes
 * *** empty log message ***
 *
 * Revision 1.4  1992/08/18  17:15:40  libes
 * rm'd extraneous error messages
 *
 * Revision 1.3  1992/06/08  18:07:35  libes
 * prettied up interface to print_objects_when_running
 */

/*************/
/* constants */
/*************/

#define DICTIONARY_NULL (Dictionary)NULL

/*****************/
/* packages used */
/*****************/

#include "hash.h"
#include "error.h"

/************/
/* typedefs */
/************/

typedef struct Hash_Table_ * Dictionary;
typedef HashEntry       DictionaryEntry;

/****************/
/* modules used */
/****************/

#include "symbol.h"

/***************************/
/* hidden type definitions */
/***************************/

/********************/
/* global variables */
/********************/

extern SC_EXPRESS_EXPORT char DICT_type;  /**< set as a side-effect of DICT lookup routines to type of object found */

/*******************************/
/* macro function definitions */
/*******************************/

#define DICTcreate(estimated_max_size)  HASHcreate(estimated_max_size)
/** should really can DICTdo_init and rename do_type_init to do_init! */
#define DICTdo_init(dict,de)        HASHlistinit((dict),(de))
#define DICTdo_type_init(dict,de,t) HASHlistinit_by_type((dict),(de),(t))
#define DICTdo_end(hash_entry)      HASHlistend(hash_entry)

/** modify dictionary entry in-place */
#define DICTchange(e,obj,sym,typ)   { \
                    (e)->data = (obj); \
                    (e)->symbol = (sym); \
                    (e)->type = (typ); \
                    }
#define DICTchange_type(e,typ)      (e)->type = (typ)


/***********************/
/* function prototypes */
/***********************/

extern SC_EXPRESS_EXPORT void     DICTinitialize PROTO( ( void ) );
extern SC_EXPRESS_EXPORT void     DICTcleanup PROTO( ( void ) );
extern SC_EXPRESS_EXPORT int      DICTdefine PROTO( ( Dictionary, char *, Generic, Symbol *, char ) );
extern SC_EXPRESS_EXPORT int      DICT_define PROTO( ( Dictionary, char *, Generic, Symbol *, char ) );
extern SC_EXPRESS_EXPORT void     DICTundefine PROTO( ( Dictionary, char * ) );
extern SC_EXPRESS_EXPORT Generic      DICTlookup PROTO( ( Dictionary, char * ) );
extern SC_EXPRESS_EXPORT Generic      DICTlookup_symbol PROTO( ( Dictionary, char *, Symbol ** ) );
extern SC_EXPRESS_EXPORT Generic      DICTdo PROTO( ( DictionaryEntry * ) );
extern SC_EXPRESS_EXPORT void     DICTprint PROTO( ( Dictionary ) );

#endif /*DICTIONARY_H*/
