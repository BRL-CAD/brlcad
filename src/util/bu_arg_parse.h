/*                            B U . H
 * BRL-CAD
 *
 * Copyright (c) 2004-2014 United States Government as represented by
 * the U.S. Army Research Laboratory.
 *
 * This library is free software; you can redistribute it and/or
 * modify it under the terms of the GNU Lesser General Public License
 * version 2.1 as published by the Free Software Foundation.
 *
 * This library is distributed in the hope that it will be useful, but
 * WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
 * Lesser General Public License for more details.
 *
 * You should have received a copy of the GNU Lesser General Public
 * License along with this file; see the file named COPYING for more
 * information.
 */

#ifndef UTIL_BU_ARG_PARSE_H
#define UTIL_BU_ARG_PARSE_H

#include "common.h"

#ifdef HAVE_STRING_H
#  include <string.h>
#endif

#include "bu.h"

#define BU_ARG_PARSE_BUFSZ 256

enum { BU_TRUE  = 1 };
enum { BU_FALSE = 0 };

enum { BU_ARG_MAGIC = 0x2189165c }; /**< arg structs */

/* all in this part of the header MUST have "C" linkage */
#ifdef __cplusplus
extern "C" {
#endif

/* types of parse arg results */
typedef enum {
  BU_ARG_PARSE_SUCCESS = 0,
  BU_ARG_PARSE_ERR
} bu_arg_parse_result_t;

/* types of arg values */
typedef enum {
  BU_ARG_UNDEFINED,
  BU_ARG_BOOL,
  BU_ARG_DOUBLE,
  BU_ARG_LONG,
  BU_ARG_STRING
} bu_arg_valtype_t;

/* TCLAP arg types */
typedef enum {
  BU_ARG_MultiArg,
  BU_ARG_MultiSwitchArg,
  BU_ARG_SwitchArg,
  BU_ARG_UnlabeledMultiArg,
  BU_ARG_UnlabeledValueArg,
  BU_ARG_ValueArg
} bu_arg_t;

typedef enum {
  BU_ARG_NOT_REQUIRED = 0,
  BU_ARG_OPTIONAL     = 0, /* an alias */
  BU_ARG_REQUIRED     = 1
} bu_arg_req_t;

typedef struct bu_arg_value {
  union u_typ {
    /* important that first field is integral type */
    long l; /* also use as bool */
    double d;
  } u;
  char buf[BU_ARG_PARSE_BUFSZ];
} bu_arg_value_t;

/* TCLAP::Arg */
/* structs for static initialization */
/* use this struct to cast an unknown bu_arg_* type for data access */
typedef struct {
  uint32_t magic;                  	 /* BU_ARG_MAGIC                              */
  bu_arg_t arg_type;                     /* enum: type of TCLAP arg                   */
  /* return data */
  bu_arg_value_t retval;                /* use for data transfer with TCLAP code     */
} bu_arg_general_t;

typedef struct {
  uint32_t magic;                  	 /* BU_ARG_MAGIC                              */
  bu_arg_t arg_type;                     /* enum: type of TCLAP arg                   */
  /* return data */
  bu_arg_value_t retval;                /* use for data transfer with TCLAP code     */

  const char *flag;                      /* the "short" option, may be empty ("")     */
  const char *name;                      /* the "long" option                         */
  const char *desc;                      /* a brief description                       */
  int def_val;                           /* always false on init                      */
} bu_arg_switch_t;

typedef struct {
  uint32_t magic;                  	 /* BU_ARG_MAGIC                              */
  bu_arg_t arg_type;                     /* enum: type of TCLAP arg                   */
  /* return data */
  bu_arg_value_t retval;                /* use for data transfer with TCLAP code     */

  const char *flag;                      /* the "short" option, may be empty ("")     */
  const char *name;                      /* the "long" option                         */
  const char *desc;                      /* a brief description                       */
  bu_arg_req_t req;                      /* bool: is arg required?                    */
  bu_arg_valtype_t val_typ;              /* enum: value type                          */
  const char *def_val;                   /* default value (if any)                    */
} bu_arg_unlabeled_value_t;

#define BU_ARG_SWITCH_INIT(_struct, _flag_str, _name_str, _desc_str) \
 _struct.magic = BU_ARG_MAGIC; \
 _struct.arg_type = BU_ARG_SwitchArg; \
 _struct.flag = _flag_str; \
 _struct.name = _name_str; \
 _struct.desc = _desc_str; \
 _struct.def_val = BU_FALSE; \
 memset(_struct.retval.buf, 0, sizeof(char) * BU_ARG_PARSE_BUFSZ);

#define BU_ARG_UNLABELED_VALUE_INIT(_struct, _flag_str, _name_str, _desc_str, \
                        _required_bool, _val_typ, _def_val_str) \
 _struct.magic = BU_ARG_MAGIC; \
 _struct.arg_type = BU_ARG_UnlabeledValueArg; \
 _struct.flag = _flag_str; \
 _struct.name = _name_str; \
 _struct.desc = _desc_str; \
 _struct.req = _required_bool; \
 _struct.val_typ = _val_typ; \
 _struct.def_val = _def_val_str; \
 memset(_struct.retval.buf, 0, sizeof(char) * BU_ARG_PARSE_BUFSZ);

/* the getters */
/* using stack buf transfer */
int bu_arg_get_bool(void *arg);
long bu_arg_get_long(void *arg);
double bu_arg_get_double(void *arg);
void bu_arg_get_string(void *arg, char buf[]);

/* the action: all in one function */
int bu_arg_parse(void *args[], int argc, char * const argv[]);


/* all in this header MUST have "C" linkage */
#ifdef __cplusplus
} /* extern "C" { */
#endif

#endif /* UTIL_BU_ARG_PARSE_H */
