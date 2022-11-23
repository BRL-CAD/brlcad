#ifndef CLASS_STRINGS_H
#define CLASS_STRINGS_H

/** \file class_strings.h
 * These functions are all required to generate class names, so they
 * are used by the configure-time schema scanner to determine what
 * filenames will be used for a given schema. They are grouped here
 * to reduce the number of source files the scanner depends upon.
 *
 * Functions prototyped here are implemented in class_strings.c
 */

#include <express/entity.h>
#include <express/type.h>

#define MAX_LEN              240
#define TYPE_PREFIX          "Sdai"
#define ENTITYCLASS_PREFIX   TYPE_PREFIX

/** \returns:  temporary copy of name suitable for use as a class name
 *  Side Effects:  erases the name created by a previous call to this function
 */
const char * ClassName( const char * oldname );

/** \returns  the name of the c++ class representing the entity */
const char * ENTITYget_classname( Entity ent );

/** supplies the type of a data member for the c++ class
 * \returns:  a string which is the type of the data member in the c++ class
 */
const char * TYPEget_ctype( const Type t );

/** name of type as defined in SDAI C++ binding  4-Nov-1993 */
const char * TypeName( Type t );

/** These functions take a character or a string and return
 ** a temporary copy of the string with the function applied to it.
 **
 ** Side Effects:  character or string returned persists until the
 ** next invocation of the function
 **
 ** \returns a temporary copy of characters
 ** @{
 */
char ToLower( char c );
char ToUpper( char c );
const char * StrToLower( const char * word );
const char * StrToUpper( const char * word );
const char * StrToConstant( const char * word );
const char * FirstToUpper( const char * word );
/* @} */
#endif /* CLASS_STRINGS_H */
