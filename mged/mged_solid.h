/*
 *  			S O L I D . H
 *
 *	Solids structure definition
 *
 *  Author -
 *	Michael John Muuss
 *  
 *  Source -
 *	SECAD/VLD Computing Consortium, Bldg 394
 *	The U. S. Army Ballistic Research Laboratory
 *	Aberdeen Proving Ground, Maryland  21005
 *  
 *  Copyright Notice -
 *	This software is Copyright (C) 1985 by the United States Army.
 *	All rights reserved.
 *
 *  $Header$
 */

#define MAX_PATH	16	/* Maximum depth of path */
struct solid  {
	fastf_t	s_size;		/* Distance across solid, in model space */
	fastf_t	s_csize;	/* Dist across clipped solid (model space) */
	vect_t	s_center;	/* Center point of solid, in model space */
	unsigned long s_addr;	/* Display processor's core address */
	unsigned s_bytes;	/* Display processor's core length */
	struct bu_list s_vlist;/* Pointer to unclipped vector list */
	int	s_vlen;		/* # of actual cmd[] entries in vlist */
	struct directory *s_path[MAX_PATH];	/* Full `path' name */
	char	s_last;		/* index of last path element */
	char	s_flag;		/* UP = object visible, DOWN = obj invis */
	char	s_iflag;	/* UP = illuminated, DOWN = regular */
	char	s_soldash;	/* solid/dashed line flag */
	char	s_Eflag;	/* flag - not a solid but an "E'd" region */
	unsigned char	s_basecolor[3];	/* color from containing region */
	unsigned char	s_color[3];	/* color to draw as */
	short	s_dmindex;	/* display manager private, for color index */
	short	s_regionid;	/* region ID */
	struct solid *s_forw;	/* forward link */
	struct solid *s_back;	/* backward link */
};
#define SOLID_NULL	((struct solid *)0)
extern struct solid	*FreeSolid;	/* Head of freelist */
extern struct solid	HeadSolid;	/* Head of doubly linked solid tab */
extern int		ndrawn;

#define GET_SOLID(p)    {  \
	if( ((p)=FreeSolid) == SOLID_NULL )  { \
		p = (struct solid *)bu_calloc(1,sizeof(struct solid),"struct solid"); \
	} else { \
		FreeSolid = (p)->s_forw; \
	} \
	BU_LIST_INIT( &((p)->s_vlist) ); \
	}

#define FREE_SOLID(p) {(p)->s_forw = FreeSolid; FreeSolid = (p); \
	RT_FREE_VLIST( &((p)->s_vlist ) ); \
	(p)->s_addr = 0; }

#define FOR_ALL_SOLIDS(p)  \
	for( p=HeadSolid.s_forw; p != &HeadSolid; p = p->s_forw )

/* Insert "new" solid in front of "old" solid */
#define INSERT_SOLID(new,old)	{ \
	(new)->s_back = (old)->s_back; \
	(old)->s_back = (new); \
	(new)->s_forw = (old); \
	(new)->s_back->s_forw = (new);  }

/* Append "new" solid after "old" solid */
#define APPEND_SOLID(new,old)	{ \
	(new)->s_forw = (old)->s_forw; \
	(new)->s_back = (old); \
	(old)->s_forw = (new); \
	(new)->s_forw->s_back = (new);  }

/* Dequeue "cur" solid from doubly-linked list */
#define DEQUEUE_SOLID(cur)	{ \
	(cur)->s_forw->s_back = (cur)->s_back; \
	(cur)->s_back->s_forw = (cur)->s_forw;  }
