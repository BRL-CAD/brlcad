/*                            B U . H
 * BRL-CAD
 *
 * Copyright (c) 2004-2013 United States Government as represented by
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

#ifndef BU_OPT_PARSE_H
#define BU_OPT_PARSE_H

/* all in this header MUST have "C" linkage */
#ifdef __cplusplus
extern "C" {
#endif

/* using ideas from Cliff */
/* types of parse arg results */
typedef enum {
  BU_ARG_PARSE_SUCCESS = 0,
  BU_ARG_PARSE_ERR
} bu_arg_parse_result_t;

/* types of arg values */
typedef enum {
  BU_ARG_BOOL,
  BU_ARG_CHAR,
  BU_ARG_DOUBLE,
  BU_ARG_FLOAT,
  BU_ARG_INT,
  BU_ARG_STRING
} bu_arg_value_t;

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
  BU_ARG_REQUIRED = 1
} bu_arg_req_t;

typedef union {
  /* important that first field is int */
  int i; /* also use as bool */
  char *s;
  char c;
  double d;
  float f;
} bu_arg_value;

/* TCLAP::Arg */
typedef struct bu_arg_vars_type {
  bu_arg_t arg_type;          /* enum: type of TCLAP arg               */
  const char *flag;           /* the "short" option, may be empty ("") */
  const char *name;           /* the "long" option                     */
  const char *desc;           /* a brief description                   */
  bu_arg_req_t req;           /* bool: is arg required?                */
  bu_arg_req_t valreq;        /* bool: is value required?              */
  bu_arg_value val;           /* union: can hold all value types       */
  bu_arg_value_t val_type;    /* enum: type in the value union         */
} bu_arg_vars;

/* the action: all in one function */
int
bu_opt_parse(bu_arg_vars *args[], int argc, char **argv);

/* all in this header MUST have "C" linkage */
#ifdef __cplusplus
} /* extern "C" { */
#endif

#endif /* BU_OPT_PARSE_H */
