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
#if __STDC__
#       define  RT_VLS_EXTERN(type_and_name,args)  extern type_and_name args
#       define  RT_VLS_ARGS(args)                  args
#else
#       define  RT_VLS_EXTERN(type_and_name,args)  extern type_and_name()
#       define  RT_VLS_ARGS(args)                  ()
#endif

struct rt_vls  {
	long	vls_magic;
	char	*vls_str;	/* Dynamic memory for buffer */
	int	vls_len;	/* Length, not counting the null */
	int	vls_max;
};
#define RT_VLS_MAGIC		0x89333bbb

#define RT_VLS_CHECK(_vls_p) { if (_vls_p->vls_magic != RT_VLS_MAGIC) { \
				fprintf(stderr, \
				"in %s at line %d RT_VLS_CHECK fails\n", \
				__FILE__, __LINE__); abort(); }}

/* This macro is used to get a pointer to the current vls string */
#define RT_VLS_ADDR(_vp)	((_vp)->vls_str)

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
RT_VLS_EXTERN(void rt_vls_bomb, (char *str, struct rt_vls *badp) );
RT_VLS_EXTERN(int rt_vls_strlen, (struct rt_vls *vp) );
RT_VLS_EXTERN(void rt_vls_trunc, (struct rt_vls *vp, int len) );
RT_VLS_EXTERN(void rt_vls_free, (struct rt_vls *vp) );
RT_VLS_EXTERN(void rt_vls_extend, (struct rt_vls *vp, int extra) );
RT_VLS_EXTERN(void rt_vls_strcpy, (struct rt_vls *vp, char *s) );
RT_VLS_EXTERN(void rt_vls_strcat, (struct rt_vls *vp, char *s) );
RT_VLS_EXTERN(void rt_vls_vlscat, (struct rt_vls *dest, struct rt_vls *src) );
RT_VLS_EXTERN(void rt_vls_vlscatzap, (struct rt_vls *dest, struct rt_vls *src) );

#endif /* SEEN_RTSTRING_H */
