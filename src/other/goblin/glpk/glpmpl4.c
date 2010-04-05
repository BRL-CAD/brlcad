/* glpmpl4.c */

/*----------------------------------------------------------------------
-- GLPMPL -- GNU MathProg Translator.
--
-- Author:        Andrew Makhorin, Department for Applied Informatics,
--                Moscow Aviation Institute, Moscow, Russia.
-- E-mail:        <mao@mai2.rcnet.ru>
-- Date written:  Spring 2003.
-- Last modified: May 2003.
--
-- This file is a part of GLPK (GNU Linear Programming Kit).
--
-- Copyright (C) 2000, 2001, 2002, 2003 Andrew Makhorin, Department
-- for Applied Informatics, Moscow Aviation Institute, Moscow, Russia.
-- All rights reserved.
--
-- GLPK is free software; you can redistribute it and/or modify it
-- under the terms of the GNU General Public License as published by
-- the Free Software Foundation; either version 2, or (at your option)
-- any later version.
--
-- GLPK is distributed in the hope that it will be useful, but WITHOUT
-- ANY WARRANTY; without even the implied warranty of MERCHANTABILITY
-- or FITNESS FOR A PARTICULAR PURPOSE. See the GNU General Public
-- License for more details.
--
-- You should have received a copy of the GNU General Public License
-- along with GLPK; see the file COPYING. If not, write to the Free
-- Software Foundation, Inc., 59 Temple Place - Suite 330, Boston, MA
-- 02111-1307, USA.
----------------------------------------------------------------------*/

#include <ctype.h>
#include <errno.h>
#include <float.h>
#include <stdarg.h>
#include <string.h>
#include "glplib.h"
#include "glpmpl.h"

/**********************************************************************/
/* * *                      MODEL STATEMENTS                      * * */
/**********************************************************************/

/*----------------------------------------------------------------------
-- execute_check - execute check statement.
--
-- This routine executes specified check statement. */

static int check_func(MPL *mpl, void *info)
{     /* this is auxiliary routine to work within domain scope */
      CHECK *chk = (CHECK *)info;
      if (!eval_logical(mpl, chk->code))
         error(mpl, "check%s failed", format_tuple(mpl, '[',
            get_domain_tuple(mpl, chk->domain)));
      return 0;
}

void execute_check(MPL *mpl, CHECK *chk)
{     loop_within_domain(mpl, chk->domain, chk, check_func);
      return;
}

/*----------------------------------------------------------------------
-- execute_display - execute display statement.
--
-- This routine executes specified display statement. */

static void display_set(MPL *mpl, SET *set, MEMBER *memb)
{     /* display member of model set */
      ELEMSET *s = memb->value.set;
      MEMBER *m;
      write_text(mpl, "%s%s%s", set->name,
         format_tuple(mpl, '[', memb->tuple),
         s->head == NULL ? " is empty" : ":");
      for (m = s->head; m != NULL; m = m->next)
         write_text(mpl, "   %s", format_tuple(mpl, '(', m->tuple));
      return;
}

static void display_par(MPL *mpl, PARAMETER *par, MEMBER *memb)
{     /* display member of model parameter */
      switch (par->type)
      {  case A_NUMERIC:
         case A_INTEGER:
         case A_BINARY:
            write_text(mpl, "%s%s = %.*g", par->name,
               format_tuple(mpl, '[', memb->tuple),
               DBL_DIG, memb->value.num);
            break;
         case A_SYMBOLIC:
            write_text(mpl, "%s%s = %s", par->name,
               format_tuple(mpl, '[', memb->tuple),
               format_symbol(mpl, memb->value.sym));
            break;
         default:
            insist(par != par);
      }
      return;
}

static void display_var(MPL *mpl, VARIABLE *var, MEMBER *memb)
{     /* display member of model variable */
      if (var->lbnd == NULL && var->ubnd == NULL)
         write_text(mpl, "%s%s",
            var->name, format_tuple(mpl, '[', memb->tuple));
      else if (var->ubnd == NULL)
         write_text(mpl, "%s%s >= %.*g",
            var->name, format_tuple(mpl, '[', memb->tuple),
            DBL_DIG, memb->value.var->lbnd);
      else if (var->lbnd == NULL)
         write_text(mpl, "%s%s <= %.*g",
            var->name, format_tuple(mpl, '[', memb->tuple),
            DBL_DIG, memb->value.var->ubnd);
      else if (var->lbnd == var->ubnd)
         write_text(mpl, "%s%s = %.*g",
            var->name, format_tuple(mpl, '[', memb->tuple),
            DBL_DIG, memb->value.var->lbnd);
      else
         write_text(mpl, "%.*g <= %s%s <= %.*g",
            DBL_DIG, memb->value.var->lbnd,
            var->name, format_tuple(mpl, '[', memb->tuple),
            DBL_DIG, memb->value.var->ubnd);
      return;
}

static void display_con(MPL *mpl, CONSTRAINT *con, MEMBER *memb)
{     /* display member of model constraint */
      FORMULA *term;
      if (con->lbnd == NULL && con->ubnd == NULL)
         write_text(mpl, "%s%s:",
            con->name, format_tuple(mpl, '[', memb->tuple));
      else if (con->ubnd == NULL)
         write_text(mpl, "%s%s >= %.*g:",
            con->name, format_tuple(mpl, '[', memb->tuple),
            DBL_DIG, memb->value.con->lbnd);
      else if (con->lbnd == NULL)
         write_text(mpl, "%s%s <= %.*g:",
            con->name, format_tuple(mpl, '[', memb->tuple),
            DBL_DIG, memb->value.con->ubnd);
      else if (con->lbnd == con->ubnd)
         write_text(mpl, "%s%s = %.*g:",
            con->name, format_tuple(mpl, '[', memb->tuple),
            DBL_DIG, memb->value.con->lbnd);
      else
         write_text(mpl, "%.*g <= %s%s <= %.*g:",
            DBL_DIG, memb->value.con->lbnd,
            con->name, format_tuple(mpl, '[', memb->tuple),
            DBL_DIG, memb->value.con->ubnd);
      if ((con->type == A_MINIMIZE || con->type == A_MAXIMIZE) &&
         memb->value.con->lbnd != 0)
         write_text(mpl, "   %.*g", DBL_DIG, - memb->value.con->lbnd);
      else if (memb->value.con->form == NULL)
         write_text(mpl, "   empty linear form");
      for (term = memb->value.con->form; term != NULL; term =
         term->next)
      {  insist(term->var != NULL);
         write_text(mpl, "   %.*g %s%s", DBL_DIG, term->coef,
            term->var->var->name, format_tuple(mpl, '[',
            term->var->memb->tuple));
      }
      return;
}

static void display_memb(MPL *mpl, CODE *code)
{     /* display member specified by pseudo-code */
      MEMBER memb;
      ARG_LIST *e;
      insist(code->op == O_MEMNUM || code->op == O_MEMSYM ||
             code->op == O_MEMSET || code->op == O_MEMVAR);
      memb.tuple = create_tuple(mpl);
      for (e = code->arg.par.list; e != NULL; e = e->next)
         memb.tuple = expand_tuple(mpl, memb.tuple, eval_symbolic(mpl,
            e->x));
      switch (code->op)
      {  case O_MEMNUM:
            memb.value.num = eval_member_num(mpl, code->arg.par.par,
               memb.tuple);
            display_par(mpl, code->arg.par.par, &memb);
            break;
         case O_MEMSYM:
            memb.value.sym = eval_member_sym(mpl, code->arg.par.par,
               memb.tuple);
            display_par(mpl, code->arg.par.par, &memb);
            delete_symbol(mpl, memb.value.sym);
            break;
         case O_MEMSET:
            memb.value.set = eval_member_set(mpl, code->arg.set.set,
               memb.tuple);
            display_set(mpl, code->arg.set.set, &memb);
            break;
         case O_MEMVAR:
            memb.value.var = eval_member_var(mpl, code->arg.var.var,
               memb.tuple);
            display_var(mpl, code->arg.var.var, &memb);
            break;
         default:
            insist(code != code);
      }
      delete_tuple(mpl, memb.tuple);
      return;
}

static void display_code(MPL *mpl, CODE *code)
{     /* display value of expression */
      switch (code->type)
      {  case A_NUMERIC:
            /* numeric value */
            {  double num;
               num = eval_numeric(mpl, code);
               write_text(mpl, "%.*g", DBL_DIG, num);
            }
            break;
         case A_SYMBOLIC:
            /* symbolic value */
            {  SYMBOL *sym;
               sym = eval_symbolic(mpl, code);
               write_text(mpl, "%s", format_symbol(mpl, sym));
               delete_symbol(mpl, sym);
            }
            break;
         case A_LOGICAL:
            /* logical value */
            {  int bit;
               bit = eval_logical(mpl, code);
               write_text(mpl, "%s", bit ? "true" : "false");
            }
            break;
         case A_TUPLE:
            /* n-tuple */
            {  TUPLE *tuple;
               tuple = eval_tuple(mpl, code);
               write_text(mpl, "%s", format_tuple(mpl, '(', tuple));
               delete_tuple(mpl, tuple);
            }
            break;
         case A_ELEMSET:
            /* elemental set */
            {  ELEMSET *set;
               MEMBER *memb;
               set = eval_elemset(mpl, code);
               if (set->head == 0)
                  write_text(mpl, "set is empty");
               for (memb = set->head; memb != NULL; memb = memb->next)
                  write_text(mpl, "   %s", format_tuple(mpl, '(',
                     memb->tuple));
            }
            break;
         case A_FORMULA:
            /* linear form */
            {  FORMULA *form, *term;
               form = eval_formula(mpl, code);
               if (form == NULL)
                  write_text(mpl, "linear form is empty");
               for (term = form; term != NULL; term = term->next)
               {  if (term->var == NULL)
                     write_text(mpl, "   %.*g", term->coef);
                  else
                     write_text(mpl, "   %.*g %s%s", DBL_DIG,
                        term->coef, term->var->var->name,
                        format_tuple(mpl, '[', term->var->memb->tuple));
               }
               delete_formula(mpl, form);
            }
            break;
         default:
            insist(code != code);
      }
      return;
}

static int display_func(MPL *mpl, void *info)
{     /* this is auxiliary routine to work within domain scope */
      DISPLAY *dpy = (DISPLAY *)info;
      DISPLAY1 *entry;
      for (entry = dpy->list; entry != NULL; entry = entry->next)
      {  if (entry->type == A_INDEX)
         {  /* dummy index */
            DOMAIN_SLOT *slot = entry->u.slot;
            write_text(mpl, "%s = %s", slot->name,
            format_symbol(mpl, slot->value));
         }
         else if (entry->type == A_SET)
         {  /* model set */
            SET *set = entry->u.set;
            MEMBER *memb;
            if (set->assign != NULL)
            {  /* the set has assignment expression; evaluate all its
                  members over entire domain */
               eval_whole_set(mpl, set);
            }
            else
            {  /* the set has no assignment expression; refer to its
                  any existing member ignoring resultant value to check
                  the data provided the data section */
               if (set->array->head != NULL)
                  eval_member_set(mpl, set, set->array->head->tuple);
            }
            /* display all members of the set array */
            if (set->array->head == NULL)
               write_text(mpl, "%s has empty content", set->name);
            for (memb = set->array->head; memb != NULL; memb =
               memb->next) display_set(mpl, set, memb);
         }
         else if (entry->type == A_PARAMETER)
         {  /* model parameter */
            PARAMETER *par = entry->u.par;
            MEMBER *memb;
            if (par->assign != NULL)
            {  /* the parameter has an assignment expression; evaluate
                  all its member over entire domain */
               eval_whole_par(mpl, par);
            }
            else
            {  /* the parameter has no assignment expression; refer to
                  its any existing member ignoring resultant value to
                  check the data provided in the data section */
               if (par->array->head != NULL)
               {  if (par->type != A_SYMBOLIC)
                     eval_member_num(mpl, par, par->array->head->tuple);
                  else
                     delete_symbol(mpl, eval_member_sym(mpl, par,
                        par->array->head->tuple));
               }
            }
            /* display all members of the parameter array */
            if (par->array->head == NULL)
               write_text(mpl, "%s has empty content", par->name);
            for (memb = par->array->head; memb != NULL; memb =
               memb->next) display_par(mpl, par, memb);
         }
         else if (entry->type == A_VARIABLE)
         {  /* model variable */
            VARIABLE *var = entry->u.var;
            MEMBER *memb;
            /* display all members of the variable array */
            if (var->array->head == NULL)
               write_text(mpl, "%s has empty content", var->name);
            for (memb = var->array->head; memb != NULL; memb =
               memb->next) display_var(mpl, var, memb);
         }
         else if (entry->type == A_CONSTRAINT)
         {  /* model constraint */
            CONSTRAINT *con = entry->u.con;
            if (entry->list == NULL)
            {  /* display the whole constraint */
               MEMBER *memb;
               eval_whole_con(mpl, con);
               if (con->array->head == NULL)
                  write_text(mpl, "%s has empty content", con->name);
               for (memb = con->array->head; memb != NULL; memb =
                  memb->next) display_con(mpl, con, memb);
            }
            else
            {  /* display the constraint member */
               TUPLE *tuple;
               ARG_LIST *e;
               ELEMCON *c;
               tuple = create_tuple(mpl);
               for (e = entry->list; e != NULL; e = e->next)
                  tuple = expand_tuple(mpl, tuple, eval_symbolic(mpl,
                     e->x));
               c = eval_member_con(mpl, con, tuple);
               delete_tuple(mpl, tuple);
               display_con(mpl, con, c->memb);
            }
         }
         else if (entry->type == A_EXPRESSION)
         {  /* expression */
            CODE *code = entry->u.code;
            if (code->op == O_MEMNUM || code->op == O_MEMSYM ||
                code->op == O_MEMSET || code->op == O_MEMVAR)
               display_memb(mpl, code);
            else
               display_code(mpl, code);
         }
         else
            insist(entry != entry);
      }
      return 0;
}

void execute_display(MPL *mpl, DISPLAY *dpy)
{     /* main display routine */
      loop_within_domain(mpl, dpy->domain, dpy, display_func);
      return;
}

/**********************************************************************/
/* * *                      GENERATING MODEL                      * * */
/**********************************************************************/

/*----------------------------------------------------------------------
-- alloc_content - allocate content arrays for all model objects.
--
-- This routine allocates content arrays for all existing model objects
-- and thereby finalizes creating model.
--
-- This routine must be called immediately after reading model section,
-- i.e. before reading data section or generating model. */

void alloc_content(MPL *mpl)
{     STATEMENT *stmt;
      /* walk through all model statements */
      for (stmt = mpl->model; stmt != NULL; stmt = stmt->next)
      {  switch (stmt->type)
         {  case A_SET:
               /* model set */
               insist(stmt->u.set->array == NULL);
               stmt->u.set->array = create_array(mpl, A_ELEMSET,
                  stmt->u.set->dim);
               break;
            case A_PARAMETER:
               /* model parameter */
               insist(stmt->u.par->array == NULL);
               switch (stmt->u.par->type)
               {  case A_NUMERIC:
                  case A_INTEGER:
                  case A_BINARY:
                     stmt->u.par->array = create_array(mpl, A_NUMERIC,
                        stmt->u.par->dim);
                     break;
                  case A_SYMBOLIC:
                     stmt->u.par->array = create_array(mpl, A_SYMBOLIC,
                        stmt->u.par->dim);
                     break;
                  default:
                     insist(stmt != stmt);
               }
               break;
            case A_VARIABLE:
               /* model variable */
               insist(stmt->u.var->array == NULL);
               stmt->u.var->array = create_array(mpl, A_ELEMVAR,
                  stmt->u.var->dim);
               break;
            case A_CONSTRAINT:
               /* model constraint/objective */
               insist(stmt->u.con->array == NULL);
               stmt->u.con->array = create_array(mpl, A_ELEMCON,
                  stmt->u.con->dim);
               break;
            case A_CHECK:
               /* check statement (has no content array) */
               break;
            case A_DISPLAY:
               /* display statement (has no content array) */
               break;
            default:
               insist(stmt != stmt);
         }
      }
      return;
}

/*----------------------------------------------------------------------
-- generate_model - generate model.
--
-- This routine executes all model statements in the order they follow
-- in the model section. */

void generate_model(MPL *mpl)
{     STATEMENT *stmt;
      insist(mpl->stmt == NULL);
      /* walk through all model statements */
      for (stmt = mpl->model; stmt != NULL; stmt = stmt->next)
      {  /* set global pointer to the statement currently executed */
         mpl->stmt = stmt;
         switch (stmt->type)
         {  case A_SET:
            case A_PARAMETER:
            case A_VARIABLE:
               /* model sets, parameters, and variables are evaluated
                  indirectly */
               break;
            case A_CONSTRAINT:
               /* evaluate model constraint or objective */
               print("Generating %s...", stmt->u.con->name);
               eval_whole_con(mpl, stmt->u.con);
               break;
            case A_CHECK:
               /* execute check statement */
               execute_check(mpl, stmt->u.chk);
               break;
            case A_DISPLAY:
               /* execute display statement */
               if (mpl->out_fp != NULL) write_text(mpl, "");
               write_text(mpl, "Display statement at line %d",
                  stmt->line);
               execute_display(mpl, stmt->u.dpy);
               break;
            default:
               insist(stmt != stmt);
         }
      }
      mpl->stmt = NULL;
      /* looks like generation has been successfully completed :+) */
      return;
}

/*----------------------------------------------------------------------
-- build_problem - build problem instance.
--
-- This routine builds lists of rows and columns for problem instance,
-- which corresponds to the generated model. */

void build_problem(MPL *mpl)
{     STATEMENT *stmt;
      MEMBER *memb;
      VARIABLE *v;
      CONSTRAINT *c;
      FORMULA *t;
      int i, j;
      insist(mpl->m == 0);
      insist(mpl->n == 0);
      insist(mpl->row == NULL);
      insist(mpl->col == NULL);
      /* check that all elemental variables has zero column numbers */
      for (stmt = mpl->model; stmt != NULL; stmt = stmt->next)
      {  if (stmt->type == A_VARIABLE)
         {  v = stmt->u.var;
            for (memb = v->array->head; memb != NULL; memb = memb->next)
               insist(memb->value.var->j == 0);
         }
      }
      /* assign row numbers to elemental constraints and objectives */
      for (stmt = mpl->model; stmt != NULL; stmt = stmt->next)
      {  if (stmt->type == A_CONSTRAINT)
         {  c = stmt->u.con;
            for (memb = c->array->head; memb != NULL; memb = memb->next)
            {  insist(memb->value.con->i == 0);
               memb->value.con->i = ++mpl->m;
               /* walk through linear form and mark elemental variables,
                  which are referenced at least once */
               for (t = memb->value.con->form; t != NULL; t = t->next)
               {  insist(t->var != NULL);
                  t->var->memb->value.var->j = -1;
               }
            }
         }
      }
      /* assign column numbers to marked elemental variables */
      for (stmt = mpl->model; stmt != NULL; stmt = stmt->next)
      {  if (stmt->type == A_VARIABLE)
         {  v = stmt->u.var;
            for (memb = v->array->head; memb != NULL; memb = memb->next)
               if (memb->value.var->j != 0) memb->value.var->j =
                  ++mpl->n;
         }
      }
      /* build list of rows */
      mpl->row = ucalloc(1+mpl->m, sizeof(ELEMCON *));
      for (i = 1; i <= mpl->m; i++) mpl->row[i] = NULL;
      for (stmt = mpl->model; stmt != NULL; stmt = stmt->next)
      {  if (stmt->type == A_CONSTRAINT)
         {  c = stmt->u.con;
            for (memb = c->array->head; memb != NULL; memb = memb->next)
            {  i = memb->value.con->i;
               insist(1 <= i && i <= mpl->m);
               insist(mpl->row[i] == NULL);
               mpl->row[i] = memb->value.con;
            }
         }
      }
      for (i = 1; i <= mpl->m; i++) insist(mpl->row[i] != NULL);
      /* build list of columns */
      mpl->col = ucalloc(1+mpl->n, sizeof(ELEMVAR *));
      for (j = 1; j <= mpl->n; j++) mpl->col[j] = NULL;
      for (stmt = mpl->model; stmt != NULL; stmt = stmt->next)
      {  if (stmt->type == A_VARIABLE)
         {  v = stmt->u.var;
            for (memb = v->array->head; memb != NULL; memb = memb->next)
            {  j = memb->value.var->j;
               if (j == 0) continue;
               insist(1 <= j && j <= mpl->n);
               insist(mpl->col[j] == NULL);
               mpl->col[j] = memb->value.var;
            }
         }
      }
      for (j = 1; j <= mpl->n; j++) insist(mpl->col[j] != NULL);
      return;
}

/*----------------------------------------------------------------------
-- clean_model - clean model content.
--
-- This routine cleans model content that assumes deleting all stuff
-- dynamically allocated during the generation phase.
--
-- Actually cleaning model content is not needed. This function is used
-- mainly to be sure that there were no logical errors on using dynamic
-- memory pools during the generation phase.
--
-- NOTE: This routine must not be called if any errors were detected on
--       the generation phase. */

void clean_model(MPL *mpl)
{     STATEMENT *stmt;
      /* check that the generation phase either has not been performed
         or has been performed successfully */
      insist(mpl->stmt == NULL);
      /* walk through all model statements */
      for (stmt = mpl->model; stmt != NULL; stmt = stmt->next)
      {  switch(stmt->type)
         {  case A_SET:
               clean_set(mpl, stmt->u.set); break;
            case A_PARAMETER:
               clean_parameter(mpl, stmt->u.par); break;
            case A_VARIABLE:
               clean_variable(mpl, stmt->u.var); break;
            case A_CONSTRAINT:
               clean_constraint(mpl, stmt->u.con); break;
            case A_CHECK:
               clean_check(mpl, stmt->u.chk); break;
            case A_DISPLAY:
               clean_display(mpl, stmt->u.dpy); break;
            default:
               insist(stmt != stmt);
         }
      }
      /* check that all atoms have been returned to their pools */
      if (mpl->strings->count != 0)
         error(mpl, "internal logic error: %d string segment(s) were lo"
            "st", mpl->strings->count);
      if (mpl->symbols->count != 0)
         error(mpl, "internal logic error: %d symbol(s) were lost",
            mpl->symbols->count);
      if (mpl->tuples->count != 0)
         error(mpl, "internal logic error: %d n-tuple component(s) were"
            " lost", mpl->tuples->count);
      if (mpl->arrays->count != 0)
         error(mpl, "internal logic error: %d array(s) were lost",
            mpl->arrays->count);
      if (mpl->members->count != 0)
         error(mpl, "internal logic error: %d array member(s) were lost"
            , mpl->members->count);
      if (mpl->elemvars->count != 0)
         error(mpl, "internal logic error: %d elemental variable(s) wer"
            "e lost", mpl->elemvars->count);
      if (mpl->formulae->count != 0)
         error(mpl, "internal logic error: %d linear term(s) were lost",
            mpl->formulae->count);
      if (mpl->elemcons->count != 0)
         error(mpl, "internal logic error: %d elemental constraint(s) w"
            "ere lost", mpl->elemcons->count);
      return;
}

/**********************************************************************/
/* * *                        INPUT/OUTPUT                        * * */
/**********************************************************************/

/*----------------------------------------------------------------------
-- open_input - open input text file.
--
-- This routine opens the input text file for scanning. */

void open_input(MPL *mpl, char *file)
{     mpl->line = 0;
      mpl->c = '\n';
      mpl->token = 0;
      mpl->imlen = 0;
      mpl->image[0] = '\0';
      mpl->value = 0.0;
      mpl->b_token = T_EOF;
      mpl->b_imlen = 0;
      mpl->b_image[0] = '\0';
      mpl->b_value = 0.0;
      mpl->f_dots = 0;
      mpl->f_scan = 0;
      mpl->f_token = 0;
      mpl->f_imlen = 0;
      mpl->f_image[0] = '\0';
      mpl->f_value = 0.0;
      memset(mpl->context, ' ', CONTEXT_SIZE);
      mpl->c_ptr = 0;
      insist(mpl->in_fp == NULL);
      mpl->in_fp = ufopen(file, "r");
      if (mpl->in_fp == NULL)
         error(mpl, "unable to open %s - %s", file, strerror(errno));
      mpl->in_file = file;
      /* scan the very first character */
      get_char(mpl);
      /* scan the very first token */
      get_token(mpl);
      return;
}

/*----------------------------------------------------------------------
-- read_char - read next character from input text file.
--
-- This routine returns a next ASCII character read from the input text
-- file. If the end of file has been reached, EOF is returned. */

int read_char(MPL *mpl)
{     int c;
      insist(mpl->in_fp != NULL);
      c = fgetc(mpl->in_fp);
      if (ferror(mpl->in_fp))
         error(mpl, "read error on %s - %s", mpl->in_file,
            strerror(errno));
      if (feof(mpl->in_fp)) c = EOF;
      return c;
}

/*----------------------------------------------------------------------
-- close_input - close input text file.
--
-- This routine closes the input text file. */

void close_input(MPL *mpl)
{     insist(mpl->in_fp != NULL);
      ufclose(mpl->in_fp);
      mpl->in_fp = NULL;
      mpl->in_file = NULL;
      return;
}

/*----------------------------------------------------------------------
-- open_output - open output text file.
--
-- This routine opens the output text file used to write all the output
-- produced by display statements. */

void open_output(MPL *mpl, char *file)
{     insist(mpl->out_fp == NULL);
      mpl->out_fp = ufopen(file, "w");
      if (mpl->out_fp == NULL)
         error(mpl, "unable to create %s - %s", file, strerror(errno));
      mpl->out_file = file;
      write_text(mpl, "Start of display output");
      return;
}

/*----------------------------------------------------------------------
-- write_text - format and write text to output text file.
--
-- This routine formats and writes a text produced by display statement
-- to the output text, or, if the file is not open, to stdout using the
-- print routine. */

void write_text(MPL *mpl, char *fmt, ...)
{     va_list arg;
      char msg[4095+1];
      va_start(arg, fmt);
      vsprintf(msg, fmt, arg);
      insist(strlen(msg) < sizeof(msg));
      va_end(arg);
      if (mpl->out_fp == NULL)
         print("%s", msg);
      else
         fprintf(mpl->out_fp, "%s\n", msg);
      return;
}

/*----------------------------------------------------------------------
-- close_output - close output text file.
--
-- This routine closes the output text file. */

void close_output(MPL *mpl)
{     insist(mpl->out_fp != NULL);
      write_text(mpl, "");
      write_text(mpl, "End of display output");
      fflush(mpl->out_fp);
      if (ferror(mpl->out_fp))
         error(mpl, "write error on %s - %s", mpl->out_file,
            strerror(errno));
      ufclose(mpl->out_fp);
      mpl->out_fp = NULL;
      mpl->out_file = NULL;
      return;
}

/**********************************************************************/
/* * *                      SOLVER INTERFACE                      * * */
/**********************************************************************/

/*----------------------------------------------------------------------
-- error - print error message and terminate model processing.
--
-- This routine formats and prints an error message and then terminates
-- model processing. */

void error(MPL *mpl, char *fmt, ...)
{     va_list arg;
      char msg[4095+1];
      va_start(arg, fmt);
      vsprintf(msg, fmt, arg);
      insist(strlen(msg) < sizeof(msg));
      va_end(arg);
      switch (mpl->phase)
      {  case 1:
         case 2:
            /* translation phase */
            print("%s:%d: %s",
               mpl->in_file == NULL ? "(unknown)" : mpl->in_file,
               mpl->line, msg);
            print_context(mpl);
            break;
         case 3:
            /* generation phase */
            print("%s:%d: %s",
               mpl->mod_file == NULL ? "(unknown)" : mpl->mod_file,
               mpl->stmt == NULL ? 0 : mpl->stmt->line, msg);
            break;
         default:
            insist(mpl != mpl);
      }
      mpl->phase = 4;
      longjmp(mpl->jump, 1);
      /* no return */
}

/*----------------------------------------------------------------------
-- warning - print warning message and continue model processing.
--
-- This routine formats and prints a warning message and returns to the
-- calling program. */

void warning(MPL *mpl, char *fmt, ...)
{     va_list arg;
      char msg[4095+1];
      va_start(arg, fmt);
      vsprintf(msg, fmt, arg);
      insist(strlen(msg) < sizeof(msg));
      va_end(arg);
      switch (mpl->phase)
      {  case 1:
         case 2:
            /* translation phase */
            print("%s:%d: warning: %s",
               mpl->in_file == NULL ? "(unknown)" : mpl->in_file,
               mpl->line, msg);
            break;
         case 3:
            /* generation phase */
            print("%s:%d: warning: %s",
               mpl->mod_file == NULL ? "(unknown)" : mpl->mod_file,
               mpl->stmt == NULL ? 0 : mpl->stmt->line, msg);
            break;
         default:
            insist(mpl != mpl);
      }
      return;
}

/*----------------------------------------------------------------------
-- mpl_initialize - create and initialize translator database.
--
-- *Synopsis*
--
-- #include "glpmpl.h"
-- MPL *mpl_initialize(void);
--
-- *Description*
--
-- The routine mpl_initialize creates and initializes the database used
-- by the GNU MathProg translator.
--
-- *Returns*
--
-- The routine returns a pointer to the database created. */

MPL *mpl_initialize(void)
{     MPL *mpl;
      mpl = umalloc(sizeof(MPL));
      /* scanning segment */
      mpl->line = 0;
      mpl->c = 0;
      mpl->token = 0;
      mpl->imlen = 0;
      mpl->image = ucalloc(MAX_LENGTH+1, sizeof(char));
      mpl->image[0] = '\0';
      mpl->value = 0.0;
      mpl->b_token = 0;
      mpl->b_imlen = 0;
      mpl->b_image = ucalloc(MAX_LENGTH+1, sizeof(char));
      mpl->b_image[0] = '\0';
      mpl->b_value = 0.0;
      mpl->f_dots = 0;
      mpl->f_scan = 0;
      mpl->f_token = 0;
      mpl->f_imlen = 0;
      mpl->f_image = ucalloc(MAX_LENGTH+1, sizeof(char));
      mpl->f_image[0] = '\0';
      mpl->f_value = 0.0;
      mpl->context = ucalloc(CONTEXT_SIZE, sizeof(char));
      memset(mpl->context, ' ', CONTEXT_SIZE);
      mpl->c_ptr = 0;
      mpl->flag_d = 0;
      /* translating segment */
      mpl->pool = dmp_create_pool(0);
      mpl->tree = avl_create_tree(NULL, avl_strcmp);
      mpl->model = NULL;
      mpl->flag_x = 0;
      mpl->as_within = 0;
      mpl->as_in = 0;
      mpl->as_binary = 0;
      /* common segment */
      mpl->strings = dmp_create_pool(sizeof(STRING));
      mpl->symbols = dmp_create_pool(sizeof(SYMBOL));
      mpl->tuples = dmp_create_pool(sizeof(TUPLE));
      mpl->arrays = dmp_create_pool(sizeof(ARRAY));
      mpl->members = dmp_create_pool(sizeof(MEMBER));
      mpl->elemvars = dmp_create_pool(sizeof(ELEMVAR));
      mpl->formulae = dmp_create_pool(sizeof(FORMULA));
      mpl->elemcons = dmp_create_pool(sizeof(ELEMCON));
      mpl->a_list = NULL;
      mpl->sym_buf = ucalloc(255+1, sizeof(char));
      mpl->sym_buf[0] = '\0';
      mpl->tup_buf = ucalloc(255+1, sizeof(char));
      mpl->tup_buf[0] = '\0';
      /* generating segment */
      mpl->stmt = NULL;
      mpl->m = 0;
      mpl->n = 0;
      mpl->row = NULL;
      mpl->col = NULL;
      /* input/output segment */
      mpl->in_fp = NULL;
      mpl->in_file = NULL;
      mpl->out_fp = NULL;
      mpl->out_file = NULL;
      /* solver interface segment */
      if (setjmp(mpl->jump)) insist(mpl != mpl);
      mpl->phase = 0;
      mpl->mod_file = NULL;
      mpl->mpl_buf = ucalloc(255+1, sizeof(char));
      mpl->mpl_buf[0] = '\0';
      return mpl;
}

/*----------------------------------------------------------------------
-- mpl_read_model - read model section and optional data section.
--
-- *Synopsis*
--
-- #include "glpmpl.h"
-- int mpl_read_model(MPL *mpl, char *file);
--
-- *Description*
--
-- The routine mpl_read_model reads model section and optionally data
-- section, which may follow the model section, from the text file,
-- whose name is the character string file, performs translating model
-- statements and data blocks, and stores all the information in the
-- translator database.
--
-- This routine should be called once after the routine mpl_initialize
-- and before other API routines.
--
-- *Returns*
--
-- The routine mpl_read_model returns one the following codes:
--
-- 1 - translation successful. The input text file contains only model
--     section. In this case the calling program may call the routine
--     mpl_read_data to read data section from another file.
-- 2 - translation successful. The input text file contains both model
--     and data section.
-- 4 - processing failed due to some errors. In this case the calling
--     program should call the routine mpl_terminate to terminate model
--     processing. */

int mpl_read_model(MPL *mpl, char *file)
{     if (mpl->phase != 0)
         fault("mpl_read_model: invalid call sequence");
      if (file == NULL)
         fault("mpl_read_model: no input filename specified");
      /* set up error handler */
      if (setjmp(mpl->jump)) goto done;
      /* translate model section */
      mpl->phase = 1;
      print("Reading model section from %s...", file);
      open_input(mpl, file);
      model_section(mpl);
      if (mpl->model == NULL)
         error(mpl, "empty model section not allowed");
      /* save name of the input text file containing model section for
         error diagnostics during the generation phase */
      mpl->mod_file = ucalloc(strlen(file)+1, sizeof(char));
      strcpy(mpl->mod_file, mpl->in_file);
      /* allocate content arrays for all model objects */
      alloc_content(mpl);
      /* optional data section may begin with the keyword 'data' */
      if (is_keyword(mpl, "data"))
      {  mpl->flag_d = 1;
         get_token(mpl /* data */);
         if (mpl->token != T_SEMICOLON)
            error(mpl, "semicolon missing where expected");
         get_token(mpl /* ; */);
         /* translate data section */
         mpl->phase = 2;
         print("Reading data section from %s...", file);
         data_section(mpl);
      }
      /* process end statement */
      end_statement(mpl);
      print("%d line%s were read", mpl->line, mpl->line == 1 ? "" : "s")
         ;
      close_input(mpl);
done: /* return to the calling program */
      return mpl->phase;
}

/*----------------------------------------------------------------------
-- mpl_read_data - read data section.
--
-- *Synopsis*
--
-- #include "glpmpl.h"
-- int mpl_read_data(MPL *mpl, char *file);
--
-- *Description*
--
-- The routine mpl_read_data reads data section from the text file,
-- whose name is the character string file, performs translating data
-- blocks, and stores the data read in the translator database.
--
-- If this routine is used, it should be called once after the routine
-- mpl_read_model and if the latter returned the code 1.
--
-- *Returns*
--
-- The routine mpl_read_data returns one of the following codes:
--
-- 2 - data section has been successfully processed.
-- 4 - processing failed due to some errors. In this case the calling
--     program should call the routine mpl_terminate to terminate model
--     processing. */

int mpl_read_data(MPL *mpl, char *file)
{     if (mpl->phase != 1)
         fault("mpl_read_data: invalid call sequence");
      if (file == NULL)
         fault("mpl_read_data: no input filename specified");
      /* set up error handler */
      if (setjmp(mpl->jump)) goto done;
      /* process data section */
      mpl->phase = 2;
      print("Reading data section from %s...", file);
      mpl->flag_d = 1;
      open_input(mpl, file);
      /* in this case the keyword 'data' is optional */
      if (is_literal(mpl, "data"))
      {  get_token(mpl /* data */);
         if (mpl->token != T_SEMICOLON)
            error(mpl, "semicolon missing where expected");
         get_token(mpl /* ; */);
      }
      data_section(mpl);
      /* process end statement */
      end_statement(mpl);
      print("%d line%s were read", mpl->line, mpl->line == 1 ? "" : "s")
         ;
      close_input(mpl);
done: /* return to the calling program */
      return mpl->phase;
}

/*----------------------------------------------------------------------
-- mpl_generate - generate model.
--
-- *Synopsis*
--
-- #include "glpmpl.h"
-- int mpl_generate(MPL *mpl, char *file);
--
-- *Description*
--
-- The routine mpl_generate generates the model using its description
-- stored in the translator database. This phase means generating all
-- variables, constraints, and objectives, performing check and display
-- statements, and building the problem instance.
--
-- The character string file specifies the name of output text file, to
-- which output produced by display statements should be written. It is
-- allowed to specify NULL, in which case the output goes to stdout via
-- the routine print.
--
-- This routine should be called once after the routine mpl_read_model
-- or mpl_read_data and if one of the latters returned the code 2.
--
-- *Returns*
--
-- The routine mpl_generate returns one of the following codes:
--
-- 3 - model has been successfully generated. In this case the callng
--     program may call other api routines to obtain components of the
--     problem instance from the translator database.
-- 4 - processing failed due to some errors. In this case the calling
--     program should call the routine mpl_terminate to terminate model
--     processing. */

int mpl_generate(MPL *mpl, char *file)
{     if (!(mpl->phase == 1 || mpl->phase == 2))
         fault("mpl_generate: invalid call sequence");
      /* set up error handler */
      if (setjmp(mpl->jump)) goto done;
      /* generate model */
      mpl->phase = 3;
      if (file != NULL) open_output(mpl, file);
      generate_model(mpl);
      if (file != NULL) close_output(mpl);
      /* build problem instance */
      build_problem(mpl);
      /* generation phase has been finished */
      print("Model has been successfully generated");
done: /* return to the calling program */
      return mpl->phase;
}

/*----------------------------------------------------------------------
-- mpl_get_prob_name - obtain problem (model) name.
--
-- *Synopsis*
--
-- #include "glpmpl.h"
-- char *mpl_get_prob_name(MPL *mpl);
--
-- *Returns*
--
-- The routine mpl_get_prob_name returns a pointer to internal buffer,
-- which contains symbolic name of the problem (model).
--
-- *Note*
--
-- Currently MathProg has no feature to assign a symbolic name to the
-- model. Therefore the routine mpl_get_prob_name tries to construct
-- such name using the name of input text file containing model section,
-- although this is not a good idea (due to portability problems). */

char *mpl_get_prob_name(MPL *mpl)
{     char *name = mpl->mpl_buf;
      char *file = mpl->mod_file;
      int k;
      if (mpl->phase != 3)
         fault("mpl_get_prob_name: invalid call sequence");
      for (;;)
      {  if (strchr(file, '/') != NULL)
            file = strchr(file, '/') + 1;
         else if (strchr(file, '\\') != NULL)
            file = strchr(file, '\\') + 1;
         else if (strchr(file, ':') != NULL)
            file = strchr(file, ':') + 1;
         else
            break;
      }
      for (k = 0; ; k++)
      {  if (k == 255) break;
         if (!(isalnum((unsigned char)*file) || *file == '_')) break;
         name[k] = *file++;
      }
      if (k == 0)
         strcpy(name, "Unknown");
      else
         name[k] = '\0';
      insist(strlen(name) <= 255);
      return name;
}

/*----------------------------------------------------------------------
-- mpl_get_num_rows - determine number of rows.
--
-- *Synopsis*
--
-- #include "glpmpl.h"
-- int mpl_get_num_rows(MPL *mpl);
--
-- *Returns*
--
-- The routine mpl_get_num_rows returns total number of rows in the
-- problem, where each row is an individual constraint or objective. */

int mpl_get_num_rows(MPL *mpl)
{     if (mpl->phase != 3)
         fault("mpl_get_num_rows: invalid call sequence");
      return mpl->m;
}

/*----------------------------------------------------------------------
-- mpl_get_num_cols - determine number of columns.
--
-- *Synopsis*
--
-- #include "glpmpl.h"
-- int mpl_get_num_cols(MPL *mpl);
--
-- *Returns*
--
-- The routine mpl_get_num_cols returns total number of columns in the
-- problem, where each column is an individual variable. */

int mpl_get_num_cols(MPL *mpl)
{     if (mpl->phase != 3)
         fault("mpl_get_num_cols: invalid call sequence");
      return mpl->n;
}

/*----------------------------------------------------------------------
-- mpl_get_row_name - obtain row name.
--
-- *Synopsis*
--
-- #include "glpmpl.h"
-- char *mpl_get_row_name(MPL *mpl, int i);
--
-- *Returns*
--
-- The routine mpl_get_row_name returns a pointer to internal buffer,
-- which contains symbolic name of i-th row of the problem. */

char *mpl_get_row_name(MPL *mpl, int i)
{     char *name = mpl->mpl_buf, *t;
      int len;
      if (mpl->phase != 3)
         fault("mpl_get_row_name: invalid call sequence");
      if (!(1 <= i && i <= mpl->m))
         fault("mpl_get_row_name: i = %d; row number out of range", i);
      strcpy(name, mpl->row[i]->con->name);
      len = strlen(name);
      insist(len <= 255);
      t = format_tuple(mpl, '[', mpl->row[i]->memb->tuple);
      while (*t)
      {  if (len == 255) break;
         name[len++] = *t++;
      }
      name[len] = '\0';
      if (len == 255) strcpy(name+252, "...");
      insist(strlen(name) <= 255);
      return name;
}

/*----------------------------------------------------------------------
-- mpl_get_row_kind - determine row kind.
--
-- *Synopsis*
--
-- #include "glpmpl.h"
-- int mpl_get_row_kind(MPL *mpl, int i);
--
-- *Returns*
--
-- The routine mpl_get_row_kind returns the kind of i-th row, which can
-- be one of the following:
--
-- MPL_ST  - non-free (constraint) row;
-- MPL_MIN - free (objective) row to be minimized;
-- MPL_MAX - free (objective) row to be maximized. */

int mpl_get_row_kind(MPL *mpl, int i)
{     int kind;
      if (mpl->phase != 3)
         fault("mpl_get_row_kind: invalid call sequence");
      if (!(1 <= i && i <= mpl->m))
         fault("mpl_get_row_kind: i = %d; row number out of range", i);
      switch (mpl->row[i]->con->type)
      {  case A_CONSTRAINT:
            kind = MPL_ST; break;
         case A_MINIMIZE:
            kind = MPL_MIN; break;
         case A_MAXIMIZE:
            kind = MPL_MAX; break;
         default:
            insist(mpl != mpl);
      }
      return kind;
}

/*----------------------------------------------------------------------
-- mpl_get_row_bnds - obtain row bounds.
--
-- *Synopsis*
--
-- #include "glpmpl.h"
-- int mpl_get_row_bnds(MPL *mpl, int i, double *lb, double *ub);
--
-- *Description*
--
-- The routine mpl_get_row_bnds stores lower and upper bounds of i-th
-- row of the problem to the locations, which the parameters lb and ub
-- point to, respectively. Besides the routine returns the type of the
-- i-th row.
--
-- If some of the parameters lb and ub is NULL, the corresponding bound
-- value is not stored.
--
-- Types and bounds have the following meaning:
--
--     Type           Bounds          Note
--    -----------------------------------------------------------
--    MPL_FR   -inf <  f(x) <  +inf   Free linear form
--    MPL_LO     lb <= f(x) <  +inf   Inequality f(x) >= lb
--    MPL_UP   -inf <  f(x) <=  ub    Inequality f(x) <= ub
--    MPL_DB     lb <= f(x) <=  ub    Inequality lb <= f(x) <= ub
--    MPL_FX           f(x)  =  lb    Equality f(x) = lb
--
-- where f(x) is the corresponding linear form of the i-th row.
--
-- If the row has no lower bound, *lb is set to zero; if the row has
-- no upper bound, *ub is set to zero; and if the row is of fixed type,
-- both *lb and *ub are set to the same value.
--
-- *Returns*
--
-- The routine returns the type of the i-th row as it is stated in the
-- table above. */

int mpl_get_row_bnds(MPL *mpl, int i, double *_lb, double *_ub)
{     ELEMCON *con;
      int type;
      double lb, ub;
      if (mpl->phase != 3)
         fault("mpl_get_row_bnds: invalid call sequence");
      if (!(1 <= i && i <= mpl->m))
         fault("mpl_get_row_bnds: i = %d; row number out of range", i);
      con = mpl->row[i];
      if (con->con->lbnd == NULL && con->con->ubnd == NULL)
         type = MPL_FR, lb = ub = 0.0;
      else if (con->con->ubnd == NULL)
         type = MPL_LO, lb = con->lbnd, ub = 0.0;
      else if (con->con->lbnd == NULL)
         type = MPL_UP, lb = 0.0, ub = con->ubnd;
      else if (con->con->lbnd != con->con->ubnd)
         type = MPL_DB, lb = con->lbnd, ub = con->ubnd;
      else
         type = MPL_FX, lb = ub = con->lbnd;
      if (_lb != NULL) *_lb = lb;
      if (_ub != NULL) *_ub = ub;
      return type;
}

/*----------------------------------------------------------------------
-- mpl_get_mat_row - obtain row of the constraint matrix.
--
-- *Synopsis*
--
-- #include "glpmpl.h"
-- int mpl_get_mat_row(MPL *mpl, int i, int ndx[], double val[]);
--
-- *Description*
--
-- The routine mpl_get_mat_row stores column indices and numeric values
-- of constraint coefficients for the i-th row to locations ndx[1], ...,
-- ndx[len] and val[1], ..., val[len], respectively, where 0 <= len <= n
-- is number of (structural) non-zero constraint coefficients, and n is
-- number of columns in the problem.
--
-- If the parameter ndx is NULL, column indices are not stored. If the
-- parameter val is NULL, numeric values are not stored.
--
-- Note that free rows may have constant terms, which are not part of
-- the constraint matrix and therefore not reported by this routine. The
-- constant term of a particular row can be obtained, if necessary, via
-- the routine mpl_get_row_c0.
--
-- *Returns*
--
-- The routine mpl_get_mat_row returns len, which is length of i-th row
-- of the constraint matrix (i.e. number of non-zero coefficients). */

int mpl_get_mat_row(MPL *mpl, int i, int ndx[], double val[])
{     FORMULA *term;
      int len = 0;
      if (mpl->phase != 3)
         fault("mpl_get_mat_row: invalid call sequence");
      if (!(1 <= i && i <= mpl->m))
         fault("mpl_get_mat_row: i = %d; row number out of range", i);
      for (term = mpl->row[i]->form; term != NULL; term = term->next)
      {  insist(term->var != NULL);
         len++;
         insist(len <= mpl->n);
         if (ndx != NULL) ndx[len] = term->var->j;
         if (val != NULL) val[len] = term->coef;
      }
      return len;
}

/*----------------------------------------------------------------------
-- mpl_get_row_c0 - obtain constant term of free row.
--
-- *Synopsis*
--
-- #include "glpmpl.h"
-- double mpl_get_row_c0(MPL *mpl, int i);
--
-- *Returns*
--
-- The routine mpl_get_row_c0 returns numeric value of constant term of
-- i-th row.
--
-- Note that only free rows may have non-zero constant terms. Therefore
-- if i-th row is not free, the routine returns zero. */

double mpl_get_row_c0(MPL *mpl, int i)
{     ELEMCON *con;
      double c0;
      if (mpl->phase != 3)
         fault("mpl_get_row_c0: invalid call sequence");
      if (!(1 <= i && i <= mpl->m))
         fault("mpl_get_row_c0: i = %d; row number out of range", i);
      con = mpl->row[i];
      if (con->con->lbnd == NULL && con->con->ubnd == NULL)
         c0 = - con->lbnd;
      else
         c0 = 0.0;
      return c0;
}

/*----------------------------------------------------------------------
-- mpl_get_col_name - obtain column name.
--
-- *Synopsis*
--
-- #include "glpmpl.h"
-- char *mpl_get_col_name(MPL *mpl, int j);
--
-- *Returns*
--
-- The routine mpl_get_col_name returns a pointer to internal buffer,
-- which contains symbolic name of j-th column of the problem. */

char *mpl_get_col_name(MPL *mpl, int j)
{     char *name = mpl->mpl_buf, *t;
      int len;
      if (mpl->phase != 3)
         fault("mpl_get_col_name: invalid call sequence");
      if (!(1 <= j && j <= mpl->n))
         fault("mpl_get_col_name: j = %d; column number out of range",
            j);
      strcpy(name, mpl->col[j]->var->name);
      len = strlen(name);
      insist(len <= 255);
      t = format_tuple(mpl, '[', mpl->col[j]->memb->tuple);
      while (*t)
      {  if (len == 255) break;
         name[len++] = *t++;
      }
      name[len] = '\0';
      if (len == 255) strcpy(name+252, "...");
      insist(strlen(name) <= 255);
      return name;
}

/*----------------------------------------------------------------------
-- mpl_get_col_kind - determine column kind.
--
-- *Synopsis*
--
-- #include "glpmpl.h"
-- int mpl_get_col_kind(MPL *mpl, int j);
--
-- *Returns*
--
-- The routine mpl_get_col_kind returns the kind of j-th column, which
-- can be one of the following:
--
-- MPL_NUM - continuous variable;
-- MPL_INT - integer variable;
-- MPL_BIN - binary variable.
--
-- Note that column kinds are defined independently on type and bounds
-- (reported by the routine mpl_get_col_bnds) of corresponding columns.
-- This means, in particular, that bounds of an integer column may be
-- fractional, or a binary column may have lower and upper bounds that
-- are not 0 and 1 (or it may have no lower/upper bound at all). */

int mpl_get_col_kind(MPL *mpl, int j)
{     int kind;
      if (mpl->phase != 3)
         fault("mpl_get_col_kind: invalid call sequence");
      if (!(1 <= j && j <= mpl->n))
         fault("mpl_get_col_kind: j = %d; column number out of range",
            j);
      switch (mpl->col[j]->var->type)
      {  case A_NUMERIC:
            kind = MPL_NUM; break;
         case A_INTEGER:
            kind = MPL_INT; break;
         case A_BINARY:
            kind = MPL_BIN; break;
         default:
            insist(mpl != mpl);
      }
      return kind;
}

/*----------------------------------------------------------------------
-- mpl_get_col_bnds - obtain column bounds.
--
-- *Synopsis*
--
-- #include "glpmpl.h"
-- int mpl_get_col_bnds(MPL *mpl, int j, double *lb, double *ub);
--
-- *Description*
--
-- The routine mpl_get_col_bnds stores lower and upper bound of j-th
-- column of the problem to the locations, which the parameters lb and
-- ub point to, respectively. Besides the routine returns the type of
-- the j-th column.
--
-- If some of the parameters lb and ub is NULL, the corresponding bound
-- value is not stored.
--
-- Types and bounds have the following meaning:
--
--     Type         Bounds         Note
--    ------------------------------------------------------
--    MPL_FR   -inf <  x <  +inf   Free (unbounded) variable
--    MPL_LO     lb <= x <  +inf   Variable with lower bound
--    MPL_UP   -inf <  x <=  ub    Variable with upper bound
--    MPL_DB     lb <= x <=  ub    Double-bounded variable
--    MPL_FX           x  =  lb    Fixed variable
--
-- where x is individual variable corresponding to the j-th column.
--
-- If the column has no lower bound, *lb is set to zero; if the column
-- has no upper bound, *ub is set to zero; and if the column is of fixed
-- type, both *lb and *ub are set to the same value.
--
-- *Returns*
--
-- The routine returns the type of the j-th column as it is stated in
-- the table above. */

int mpl_get_col_bnds(MPL *mpl, int j, double *_lb, double *_ub)
{     ELEMVAR *var;
      int type;
      double lb, ub;
      if (mpl->phase != 3)
         fault("mpl_get_col_bnds: invalid call sequence");
      if (!(1 <= j && j <= mpl->n))
         fault("mpl_get_col_bnds: j = %d; column number out of range",
            j);
      var = mpl->col[j];
      if (var->var->lbnd == NULL && var->var->ubnd == NULL)
         type = MPL_FR, lb = ub = 0.0;
      else if (var->var->ubnd == NULL)
         type = MPL_LO, lb = var->lbnd, ub = 0.0;
      else if (var->var->lbnd == NULL)
         type = MPL_UP, lb = 0.0, ub = var->ubnd;
      else if (var->var->lbnd != var->var->ubnd)
         type = MPL_DB, lb = var->lbnd, ub = var->ubnd;
      else
         type = MPL_FX, lb = ub = var->lbnd;
      if (_lb != NULL) *_lb = lb;
      if (_ub != NULL) *_ub = ub;
      return type;
}

/*----------------------------------------------------------------------
-- mpl_terminate - free all resources used by translator.
--
-- *Synopsis*
--
-- #include "glpmpl.h"
-- void mpl_terminate(MPL *mpl);
--
-- *Description*
--
-- The routine mpl_terminate frees all the resources used by the GNU
-- MathProg translator. */

void mpl_terminate(MPL *mpl)
{     if (setjmp(mpl->jump)) insist(mpl != mpl);
      switch (mpl->phase)
      {  case 0:
         case 1:
         case 2:
         case 3:
            /* there were no errors; clean the model content */
            clean_model(mpl);
            insist(mpl->a_list == NULL);
            break;
         case 4:
            /* model processing has been finihed due to error; delete
               search trees, which may be created for some arrays */
            {  ARRAY *a;
               for (a = mpl->a_list; a != NULL; a = a->next)
                  if (a->tree != NULL) avl_delete_tree(a->tree);
            }
            break;
         default:
            insist(mpl != mpl);
      }
      /* delete the translator database */
      ufree(mpl->image);
      ufree(mpl->b_image);
      ufree(mpl->f_image);
      ufree(mpl->context);
      dmp_delete_pool(mpl->pool);
      avl_delete_tree(mpl->tree);
      dmp_delete_pool(mpl->strings);
      dmp_delete_pool(mpl->symbols);
      dmp_delete_pool(mpl->tuples);
      dmp_delete_pool(mpl->arrays);
      dmp_delete_pool(mpl->members);
      dmp_delete_pool(mpl->elemvars);
      dmp_delete_pool(mpl->formulae);
      dmp_delete_pool(mpl->elemcons);
      ufree(mpl->sym_buf);
      ufree(mpl->tup_buf);
      if (mpl->row != NULL) ufree(mpl->row);
      if (mpl->col != NULL) ufree(mpl->col);
      if (mpl->in_fp != NULL) ufclose(mpl->in_fp);
      if (mpl->out_fp != NULL) ufclose(mpl->out_fp);
      if (mpl->mod_file != NULL) ufree(mpl->mod_file);
      ufree(mpl->mpl_buf);
      ufree(mpl);
      return;
}

/* eof */
