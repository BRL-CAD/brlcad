/*
 *			P R O T O C O L . H
 *
 */

/*
 *  This structure is used for MSG_PIXELS messages
 */
struct line_info  {
	int	li_len;
	int	li_y;
	int	li_nrays;
	double	li_cpusec;
	/* A scanline it attached after here */
};

#define LINE_INFO(x)	(stroff_t)&(((struct line_info *)0)->x)
struct imexport desc_line_info[] =  {
	"len",		(stroff_t)0,		1,
	"%d",		LINE_INFO(li_y),	1,
	"%d",		LINE_INFO(li_nrays),	1,
	"%f",		LINE_INFO(li_cpusec),	1,
	"",		(stroff_t)0,		0
};
