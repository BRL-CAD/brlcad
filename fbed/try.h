/*
	SCCS id:	%Z% %M%	%I%
	Last edit: 	%G% at %U%
	Retrieved: 	%H% at %T%
	SCCS archive:	%P%

	Author:		Gary S. Moss
			U. S. Army Ballistic Research Laboratory
			Aberdeen Proving Ground
			Maryland 21005
			(301)278-6647 or AV-283-6647
 */
#define INCL_TRY
typedef struct
	{
	int	(*f_func)();		/* Function pointer.		*/
	char	*f_buff;		/* Macro key-stroke buffer.	*/
	char	*f_name;		/* Function/macro name.		*/
	}
Func_Tab;
#define FT_NULL	(Func_Tab *) NULL

typedef union try
	{
	struct
		{
		int		t_curr;  /* Current letter.		*/
		union try	*t_altr; /* Alternate letter node link.	*/
		union try	*t_next; /* Next letter node link.	*/
		}
	n;
	struct
		{
		Func_Tab	*t_ftbl; /* Function table pointer.	*/
		union try	*t_altr; /* Alternate letter node link.	*/
		union try	*t_next; /* Next letter node link.	*/
		}
	l;
	}
Try;
#define TRY_NULL	(Try *) NULL
