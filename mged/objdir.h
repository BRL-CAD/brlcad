
/*
 * The object directory
 */
struct directory  {
	char		*d_namep;	/* pointer to name string */
	long		d_addr;		/* disk address in obj file */
	short		d_flags;	/* flags */
	short		d_len;		/* # of db granules used by obj */
	struct directory *d_forw;	/* forward link */
};
#define DIR_NULL	((struct directory *) NULL)

#define DIR_SOLID	0x1		/* this name is a solid */
#define DIR_COMB	0x2		/* combination */
#define DIR_REGION	0x4		/* region */
#define DIR_BRANCH	0x8		/* branch name */

#ifndef NAMESIZE
#define NAMESIZE		16
#endif
