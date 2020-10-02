#ifndef OBJECT_H
#define OBJECT_H

/** **********************************************************************
** Module:  Object \file object.h
** Description: This module implements the Object abstraction.
** Constants:
**  OBJECT_NULL - the null class
**
************************************************************************/

/*
 * This software was developed by U.S. Government employees as part of
 * their official duties and is not subject to copyright.
 *
 * $Log: object.h,v $
 * Revision 1.9  1997/01/21 19:17:11  dar
 * made C++ compatible
 *
 * Revision 1.8  1995/05/17  14:27:40  libes
 * Exported UNK_get_symbol.
 *
 * Revision 1.7  1993/10/15  18:49:23  libes
 * CADDETC certified
 *
 * Revision 1.6  1993/01/19  22:17:27  libes
 * *** empty log message ***
 *
 * Revision 1.5  1992/08/18  17:15:40  libes
 * rm'd extraneous error messages
 *
 * Revision 1.4  1992/06/08  18:07:35  libes
 * prettied up interface to print_objects_when_running
 *
 * Revision 1.3  1992/05/31  23:33:15  libes
 * implemented ALIAS resolution
 *
 * Revision 1.2  1992/05/31  08:36:48  libes
 * multiple files
 *
 * Revision 1.1  1992/05/28  03:56:02  libes
 * Initial revision
 */

/*************/
/* constants */
/*************/

/** each object has an index - current the index is a mnemonic character
 * hence the array is rather sparse, but the extra slop is only a hundred
 * or so bytes, so it's nothing to sweat about */
#define MAX_OBJECT_TYPES    127 /* really could be a LOT less */

/*****************/
/* packages used */
/*****************/

#include <sc_export.h>
#include "basic.h"  /* get basic definitions */
#include "symbol.h"

/************/
/* typedefs */
/************/

/***************************/
/* hidden type definitions */
/***************************/

struct Object {
    struct Symbol_ * ( *get_symbol )();
    char * type; /**< should complete the phrase "X is ..." - i.e., "an entity", "a type", "of unknown type" */
    int bits;   /**< a bitwise selector of a type, i.e. OBJ_XX_BITS */
};

/****************/
/* modules used */
/****************/

/********************/
/* global variables */
/********************/

extern SC_EXPRESS_EXPORT struct Object * OBJ;

/******************************/
/* macro function definitions */
/******************************/

#define OBJget_symbol(obj,type)     (*OBJ[(int)type].get_symbol)(obj)
#define OBJget_type(_type_)     (OBJ[(int)_type_].type)
#define OBJget_bits(_type_)     (OBJ[(int)_type_].bits)
#define OBJtype_is_oneof(_type_,class)  (OBJ[(int)_type_].bits & (class))
#define OBJget_name(obj,type)       (OBJget_symbol(obj,type)->name)

/***********************/
/* function prototypes */
/***********************/

extern SC_EXPRESS_EXPORT void OBJinitialize PROTO( ( void ) );
extern SC_EXPRESS_EXPORT void OBJcleanup PROTO( ( void ) );
extern SC_EXPRESS_EXPORT void OBJcreate PROTO( ( char, struct Symbol_ * ( * )( Generic ), char *, int ) );
extern SC_EXPRESS_EXPORT Symbol * UNK_get_symbol PROTO( ( Generic x ) );

#endif /*OBJECT_H*/
