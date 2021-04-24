/*
 * Mesa 3-D graphics library
 * Version:  6.5.2
 *
 * Copyright (C) 2005-2006  Brian Paul   All Rights Reserved.
 *
 * Permission is hereby granted, free of charge, to any person obtaining a
 * copy of this software and associated documentation files (the "Software"),
 * to deal in the Software without restriction, including without limitation
 * the rights to use, copy, modify, merge, publish, distribute, sublicense,
 * and/or sell copies of the Software, and to permit persons to whom the
 * Software is furnished to do so, subject to the following conditions:
 *
 * The above copyright notice and this permission notice shall be included
 * in all copies or substantial portions of the Software.
 *
 * THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS
 * OR IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
 * FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT.  IN NO EVENT SHALL
 * BRIAN PAUL BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER LIABILITY, WHETHER IN
 * AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM, OUT OF OR IN
 * CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN THE SOFTWARE.
 */

#ifndef SLANG_COMPILE_FUNCTION_H
#define SLANG_COMPILE_FUNCTION_H


struct slang_code_unit_;

/**
 * Types of functions.
 */
typedef enum slang_function_kind_
{
   SLANG_FUNC_ORDINARY,
   SLANG_FUNC_CONSTRUCTOR,
   SLANG_FUNC_OPERATOR
} slang_function_kind;


/**
 * When we need to fill in addresses which we won't know until the future,
 * we keep track of them with a fix-up table.
 */
typedef struct slang_fixup_table_
{
   GLuint *table;     /**< array[count] of addresses */
   GLuint count;
} slang_fixup_table;

extern void slang_fixup_table_init(slang_fixup_table *);
extern void slang_fixup_table_free(slang_fixup_table *);
extern GLboolean slang_fixup_save(slang_fixup_table *fixups, GLuint address);


/**
 * Description of a compiled shader function.
 */
typedef struct slang_function_
{
   slang_function_kind kind;
   slang_variable header;      /**< The function's name and return type */
   slang_variable_scope *parameters; /**< formal parameters AND local vars */
   unsigned int param_count;   /**< number of formal params (no locals) */
   slang_operation *body;      /**< The instruction tree */
   unsigned int address;       /**< Address of this func in memory */
   slang_fixup_table fixups;   /**< Mem locations which need func's address */
} slang_function;

extern int slang_function_construct(slang_function *);
extern void slang_function_destruct(slang_function *);


/**
 * Basically, a list of compiled functions.
 */
typedef struct slang_function_scope_
{
   slang_function *functions;
   GLuint num_functions;
   struct slang_function_scope_ *outer_scope;
} slang_function_scope;


extern GLvoid
_slang_function_scope_ctr(slang_function_scope *);

extern void
slang_function_scope_destruct(slang_function_scope *);

extern GLboolean
_slang_function_has_return_value(const slang_function *fun);

extern int
slang_function_scope_find_by_name(slang_function_scope *, slang_atom, int);

extern slang_function *
slang_function_scope_find(slang_function_scope *, slang_function *, int);

#endif /* SLANG_COMPILE_FUNCTION_H */
