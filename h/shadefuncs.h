/*
 *			M A T E R I A L . H
 */

#ifdef cray
/*
 * Cray has a problem taking the address of an arbitrary character within
 * a structure.  int pointers have to be used instead.
 * There is some matching hackery in the invididual tables.
 */
typedef int	*mp_off_ty;
#else
typedef char	*mp_off_ty;
#endif

struct matparse {
	char		*mp_name;
	mp_off_ty	mp_offset;
	char		*mp_fmt;
};
