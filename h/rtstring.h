/*
 *			R T S T R I N G . H
 *
 *  Definitions and macros for the RT variable length string routines.
 *
 *  Author -
 *	Michael John Muuss
 *
 *
 *  Source -
 *	SECAD/VLD Computing Consortium, Bldg 394
 *	The U. S. Army Ballistic Research Laboratory
 *	Aberdeen Proving Ground, Maryland  21005-5066
 *  
 *  Distribution Status -
 *      Public Domain, Distribution Unlimitied.
 *
 *  $Header$
 */

#ifndef SEEN_RTSTRING_H
#define SEEN_RTSTRING_H yes
/*
 *  Macros for providing function prototypes, regardless of whether
 *  the compiler understands them or not.
 *  It is vital that the argument list given for "args" be enclosed
 *  in parens.
 */
#if __STDC__ || defined(USE_PROTOTYPES)
#       define  RT_VLS_EXTERN(type_and_name,args)  extern type_and_name args
#       define  RT_VLS_ARGS(args)                  args
#else
#       define  RT_VLS_EXTERN(type_and_name,args)  extern type_and_name()
#       define  RT_VLS_ARGS(args)                  ()
#endif

struct rt_vls  {
	long	vls_magic;
	char	*vls_str;	/* Dynamic memory for buffer */
	int	vls_offset;	/* Offset into vls_str where data is good */
	int	vls_len;	/* Length, not counting the null */
	int	vls_max;
};
#define RT_VLS_MAGIC		0x89333bbb

#define RT_VLS_CHECK(_vp) { if ( !(_vp) || (_vp)->vls_magic != RT_VLS_MAGIC) { \
				fprintf(stderr, \
				"in %s at line %d RT_VLS_CHECK fails\n", \
				__FILE__, __LINE__); \
				rt_vls_bomb("RT_VLS_CHECK", _vp); }}

/*
 *			R T _ V L S _ A D D R
 *
 *  Used to get a pointer to a vls string.
 *  This macro is obsolete, and just referrs to the subroutine.
 */
#define RT_VLS_ADDR(_vp)	rt_vls_addr(_vp)

/*
 *			R T _ V L S _ I N I T
 *
 *  Used to initialize VLS string structures.
 *  This macro is obsolete, and just referrs to the subroutine.
 */
#define RT_VLS_INIT(_vp)	rt_vls_init(_vp)

/*
 *  Subroutine declarations
 */
RT_VLS_EXTERN(void rt_vls_init, (struct rt_vls *vp) );
RT_VLS_EXTERN(char *rt_vls_addr, (struct rt_vls *vp) );
RT_VLS_EXTERN(void rt_vls_extend, (struct rt_vls *vp, int extra) );
RT_VLS_EXTERN(int rt_vls_strlen, (CONST struct rt_vls *vp) );
RT_VLS_EXTERN(void rt_vls_trunc, (struct rt_vls *vp, int len) );
RT_VLS_EXTERN(void rt_vls_nibble, (struct rt_vls *vp, int len) );
RT_VLS_EXTERN(void rt_vls_free, (struct rt_vls *vp) );
RT_VLS_EXTERN(void rt_vls_strcpy, (struct rt_vls *vp, CONST char *s) );
RT_VLS_EXTERN(void rt_vls_strncpy, (struct rt_vls *vp, CONST char *s, int n) );
RT_VLS_EXTERN(void rt_vls_strcat, (struct rt_vls *vp, CONST char *s) );
RT_VLS_EXTERN(void rt_vls_strncat, (struct rt_vls *vp, CONST char *s, int n) );
RT_VLS_EXTERN(void rt_vls_vlscat, (struct rt_vls *dest, CONST struct rt_vls *src) );
RT_VLS_EXTERN(void rt_vls_vlscatzap, (struct rt_vls *dest, struct rt_vls *src) );
RT_VLS_EXTERN(void rt_vls_from_argv, (struct rt_vls *vp, int argc, char **argv) );
RT_VLS_EXTERN(void rt_vls_bomb, (CONST char *str, CONST struct rt_vls *badp) );
RT_VLS_EXTERN(void rt_vls_fwrite, (FILE *fp, CONST struct rt_vls *vp) );
RT_VLS_EXTERN(int rt_vls_gets, (struct rt_vls *vp, FILE *fp) );
RT_VLS_EXTERN(void rt_vls_putc, (struct rt_vls *vp, int c) );
RT_VLS_EXTERN(void rt_vls_printf, (struct rt_vls *vls, char *fmt, ... ) );
RT_VLS_EXTERN(void rt_vls_blkset, (struct rt_vls *vp, int len, int ch) );


#endif /* SEEN_RTSTRING_H */
