/** @file expprint.h
 *
 * Routines for examining express parser state.
 *
 */

#ifndef EXP_PRINT_H
#define EXP_PRINT_H

#include "express/express.h"

/** print the contents of an Express object to stdout
 *  - useful in examining parser behavior
 */
void expprintExpress(Express exp);

/** print a string representation of tokenID to stdout
 *  - useful in examining lexer behavior
 */
void expprintToken(int tokenID);

#endif
