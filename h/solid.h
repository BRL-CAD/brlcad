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
  struct bu_list l;
  fastf_t s_size;	/* Distance across solid, in model space */
  fastf_t s_csize;	/* Dist across clipped solid (model space) */
  vect_t s_center;	/* Center point of solid, in model space */
  struct bu_list s_vlist;/* Pointer to unclipped vector list */
  int s_vlen;		/* # of actual cmd[] entries in vlist */
  struct directory *s_path[MAX_PATH];	/* Full `path' name */
  char s_last;		/* index of last path element */
  char s_flag;		/* UP = object visible, DOWN = obj invis */
  char s_iflag;	        /* UP = illuminated, DOWN = regular */
  char s_soldash;	/* solid/dashed line flag */
  char s_Eflag;	        /* flag - not a solid but an "E'd" region */
  unsigned char	s_basecolor[3];	/* color from containing region */
  unsigned char	s_color[3];	/* color to draw as */
  short	s_regionid;	/* region ID */
  unsigned int s_dlist; /* display list index */
};

/*
 * "Standard" flag settings
 */
#define UP	0
#define DOWN	1

#define SOLID_NULL	((struct solid *)0)

#define GET_SOLID(p,fp) { \
	if(BU_LIST_IS_EMPTY(fp)){ \
		BU_GETSTRUCT(p,solid); \
	}else{ \
		p = BU_LIST_NEXT(solid,fp); \
		BU_LIST_DEQUEUE(&((p)->l)); \
	} \
	BU_LIST_INIT( &((p)->s_vlist) ); }

#define FREE_SOLID(p,fp) { \
	BU_LIST_APPEND(fp, &((p)->l)); \
	RT_FREE_VLIST(&((p)->s_vlist)); }

#define FOR_ALL_SOLIDS(p,hp)  \
	for(BU_LIST_FOR(p,solid,hp))

#define FOR_REST_OF_SOLIDS(p1,p2,hp) \
	for(BU_LIST_PFOR(p1,p2,solid,hp))

/*XXX Eventually move to bu.h */
#define BU_LIST_PREV_IS_HEAD(p,hp)\
	(((struct bu_list *)(p))->back == (hp))

#define BU_LIST_PFOR(p1,p2,structure,hp) \
	(p1)=BU_LIST_PNEXT(structure,p2); \
	BU_LIST_NOT_HEAD(p1,hp);\
	(p1)=BU_LIST_PNEXT(structure,p1)
     
