/*
 *			C O M B _ S T D . C
 *
 * Functions -
 *	bt_create_leaf		Create a leaf node for a Boolean tree
 *	bt_create_internal	Create an internal node for a Boolean tree
 *	f_comb_std		Create or extend a combination from
 *				a Boolean expression in unrestricted
 *				(standard) form
 *	bool_input_from_vls	input function for lex(1) scanner
 *	bool_unput_from_vls	output function for lex(1) scanner
 *
 *  Author -
 *	Paul Tanenbaum
 *
 *  Source -
 *	The U. S. Army Research Laboratory
 *	Aberdeen Proving Ground, Maryland  21005
 *
 *  Distribution Notice -
 *	Re-distribution of this software is restricted, as described in
 *	your "Statement of Terms and Conditions for the Release of
 *	The BRL-CAD Package" agreement.
 *
 *  Copyright Notice -
 *	This software is Copyright (C) 1995 by the United States Army
 *	in all countries except the USA.  All rights reserved.
 */
#ifndef lint
static char RCSid[] = "@(#)$Header$ (BRL)";
#endif

#include "conf.h"

#include <stdio.h>

#include "machine.h"
#include "vmath.h"
#include "raytrace.h"
#include "rtgeom.h"
#include "./ged.h"
#include "./comb_bool.h"

#define	PRINT_USAGE Tcl_AppendResult(interp, "c: usage 'c [-gr] comb_name [bool_expr]'\n",\
				     (char *)NULL)

static struct bu_vls	vp;			/* The Boolean expression */
char			*bool_bufp = 0;
struct bool_tree_node	*comb_bool_tree;

/*
 *		    B T _ C R E A T E _ L E A F ( )
 *
 *		Create a leaf node for a Boolean tree
 *
 */
struct bool_tree_node *
bt_create_leaf(object_name)
char *object_name;
{
    struct bool_tree_node	*b;
    char			*sp;

    b = (struct bool_tree_node *)
	    bu_malloc(sizeof(struct bool_tree_node), "Bool-tree leaf");

    b -> btn_magic = BOOL_TREE_NODE_MAGIC;
    bt_opn(b) = OPN_NULL;
    sp = bu_malloc(strlen(object_name) + 1, "Bool-tree leaf name");

    bu_semaphore_acquire( (unsigned int)(&rt_g.res_syscall - &rt_g.res_syscall) );		/* lock */
    sprintf(sp, "%s", object_name);
    bu_semaphore_release( (unsigned int)(&rt_g.res_syscall - &rt_g.res_syscall) );		/* unlock */

    bt_leaf_name(b) = sp;

    return(b);
}

/*
 *		B T _ C R E A T E _ I N T E R N A L ( )
 *
 *		Create a leaf node for a Boolean tree
 *
 */
struct bool_tree_node *
bt_create_internal(opn, opd1, opd2)
int opn;
struct bool_tree_node *opd1, *opd2;
{
    struct bool_tree_node	*b;

    BU_CKMAG(opd1, BOOL_TREE_NODE_MAGIC, "Boolean tree node");
    BU_CKMAG(opd2, BOOL_TREE_NODE_MAGIC, "Boolean tree node");

    b = (struct bool_tree_node *)
	    bu_malloc(sizeof(struct bool_tree_node), "Bool-tree leaf");

    b -> btn_magic = BOOL_TREE_NODE_MAGIC;
    switch (opn)
    {
	case OPN_UNION:
	case OPN_DIFFERENCE:
	case OPN_INTERSECTION:
	    bt_opn(b) = opn;
	    break;
	default:
	    bu_log("%s:%d:bt_create_internal: Illegal operation '%d'\n",
		__FILE__, __LINE__, opn);
	    exit (1);
    }
    bt_opd(b, BT_LEFT) = opd1;
    bt_opd(b, BT_RIGHT) = opd2;

    return (b);
}


/*
 *		    F _ C O M B _ S T D ( )
 *
 *	Input a combination in standard set-theoetic notation
 *
 *	Syntax: c [-gr] comb_name [boolean_expr]
 */
int
f_comb_std(clientData, interp, argc, argv)
ClientData clientData;
Tcl_Interp *interp;
int	argc;
char	**argv;
{
    char			*comb_name;
    int				ch;
    int				region_flag = -1;
    register struct directory	*dp;

    extern int			optind;
    extern char			*optarg;

    if(mged_cmd_arg_check(argc, argv, (struct funtab *)NULL))
      return TCL_ERROR;

    /* Parse options */
    optind = 1;	/* re-init getopt() */
    while ((ch = getopt(argc, argv, "gr?")) != EOF)
    {
	switch (ch)
	{
	    case 'g':
		region_flag = 0;
		break;
	    case 'r':
		region_flag = 1;
		break;
	    case '?':
	    default:
		PRINT_USAGE;
		return TCL_OK;
	}
    }
    argc -= (optind + 1);
    argv += optind;

    comb_name = *argv++;
    if (argc == -1)
    {
	PRINT_USAGE;
	return TCL_OK;
    }

    if ((region_flag != -1) && (argc == 0))
    {
    	struct rt_db_internal intern;
	struct rt_comb_internal *comb;

	/*
	 *	Set/Reset the REGION flag of an existing combination
	 */
	if ((dp = db_lookup(dbip, comb_name, LOOKUP_NOISY)) == DIR_NULL)
	  return TCL_ERROR;

    	if( rt_db_get_internal( &intern, dp, dbip, (mat_t *)NULL ) < 0 )
    		TCL_READ_ERR_return;
    	comb = (struct rt_comb_internal *)intern.idb_ptr;
    	RT_CK_COMB( comb );

    	if( region_flag )
    		comb->region_flag = 1;
    	else
    		comb->region_flag = 0;

    	if( rt_db_put_internal( dp, dbip, &intern ) < 0 )
    		TCL_WRITE_ERR_return;

	return TCL_OK;
    }
    /*
     *	At this point, we know we have a Boolean expression.
     *	If the combination already existed and region_flag is -1,
     *	then leave its c_flags alone.
     *	If the combination didn't exist yet,
     *	then pretend region_flag was 0.
     *	Otherwise, make sure to set its c_flags according to region_flag.
     */

    if (bool_bufp == 0)
    {
	bu_vls_init(&vp);
	bool_bufp = bu_vls_addr(&vp);
    }
    else
	bu_vls_trunc(&vp, 0);

    bu_vls_from_argv(&vp, argc, argv);
    Tcl_AppendResult(interp, "Will define ", (region_flag ? "region" : "group"),
		     " '", comb_name, "' as '", bool_bufp, "'\n", (char *)NULL);
    if (yyparse() != 0)
    {
      Tcl_AppendResult(interp, "Invalid Boolean expression\n");
      return TCL_ERROR;
    }
    show_gift_bool(comb_bool_tree, 1);

    return TCL_OK;
}

/*
 *		B O O L _ I N P U T _ F R O M _ V L S ( )
 *
 *	Input function for LEX(1) scanner to read Boolean expressions
 *	from a variable-length string.
 */
int
bool_input_from_vls()
{
    if (*bool_bufp == '\0')
	return (0);
    else
	return (*bool_bufp++);
}

/*
 *		B O O L _ U N P U T _ F R O M _ V L S ( )
 *
 *	Unput function for LEX(1) scanner to read Boolean expressions
 *	from a variable-length string.
 */
void
bool_unput_from_vls(ch)
int ch;
{
    *--bool_bufp = ch;
}
