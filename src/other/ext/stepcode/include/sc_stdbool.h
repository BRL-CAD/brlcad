#ifndef STDBOOL_H_
#define STDBOOL_H_

/**
 * stdbool.h - ISO C99 Boolean type
 * Author    - Bill Chatfield
 * E-mail    - bill underscore chatfield at yahoo dot com
 * Copyright - You are free to use for any purpose except illegal acts
 * Warrenty  - None: don't blame me if it breaks something
 *
 * In ISO C99, stdbool.h is a standard header and _Bool is a keyword, but
 * some compilers don't offer these yet. This header file is an
 * implementation of the standard ISO C99 stdbool.h header file. It checks
 * for various compiler versions and defines things that are missing in
 * those versions.
 *
 * See http://predef.sourceforge.net/precomp.html for compile macros.
 */

#ifndef __cplusplus

/**
 * Microsoft C/C++ version 14.00.50727.762, which comes with Visual C++ 2005,
 * and version 15.00.30729.01, which comes with Visual C++ 2008, do not
 * define _Bool.
 */
#if defined(_MSC_VER) && _MSC_VER<1800
typedef int _Bool;
#endif

/**
 * Define the Boolean macros only if they are not already defined.
 */
#ifndef __bool_true_false_are_defined
#define bool _Bool
#define false 0
#define true 1
#define __bool_true_false_are_defined 1
#endif

#endif /* __cplusplus */

#endif /*STDBOOL_H_*/
