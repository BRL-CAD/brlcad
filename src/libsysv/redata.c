/*
 *			R E D A T A . C
 *
 *  Message strings for:
 *  An interface to the System-V regular expression subroutines which
 *  present the Berkeley (BSD) names and semantics, so that all
 *  regular expression code can be programmed using the Berkeley interface.
 *
 *	last edit:	04-Nov-1987	D A Gwyn
 *
 *  Author -
 *	D A Gwyn
 *  
 *  Source -
 *	SECAD/VLD Computing Consortium, Bldg 394
 *	The U. S. Army Ballistic Research Laboratory
 *	Aberdeen Proving Ground, Maryland  21005-5066
 *  
 */
#ifndef lint
static const char RCSid[] = "@(#)$Header$ (BRL)";
#endif

#if defined(BSD)
#ifndef lint
char	redata_dummy;      /* some systems can't handle empty object modules */
#else
		/* Nothing further to do on BSD systems */
#endif
#else

struct	{
	int	number;
	char	*message;
	}	re_msgtab[] =
	{
	{	11,	"range endpoint too large"	},
	{	16,	"bad number"	},
	{	25,	"`\\digit' out of range"	},
	{	35,	"no match"	},
	{	36,	"illegal or missing delimiter"	},
	{	41,	"no remembered search string"	},
	{	42,	"'\\( \\)' imbalance"	},
	{	43,	"too many `\\(' s"	},
	{	44,	"more than 2 numbers given"	},
	{	45,	"'\\}' expected"	},
	{	46,	"first number exceeds second"	},
	{	49,	"'[ ]' imbalance"	},
	{	50,	"regular expression overflow"	},
	{	51,	"regular expression error"	},
	{	0,	"unknown r.e. error"	}
	};

#endif

/*
 * Local Variables:
 * mode: C
 * tab-width: 8
 * c-basic-offset: 4
 * indent-tabs-mode: t
 * End:
 * ex: shiftwidth=4 tabstop=8
 */
