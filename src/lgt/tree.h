/*                          T R E E . H
 * BRL-CAD
 *
 * Copyright (c) 2004-2011 United States Government as represented by
 * the U.S. Army Research Laboratory.
 *
 * This program is free software; you can redistribute it and/or
 * modify it under the terms of the GNU Lesser General Public License
 * version 2.1 as published by the Free Software Foundation.
 *
 * This program is distributed in the hope that it will be useful, but
 * WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
 * Lesser General Public License for more details.
 *
 * You should have received a copy of the GNU Lesser General Public
 * License along with this file; see the file named COPYING for more
 * information.
 */
/** @file lgt/tree.h
    Author:		Gary S. Moss
*/

#define INCL_TREE
typedef struct octree		Octree;
typedef struct octreeplist	OcList;
typedef union trie		Trie;
typedef struct pointlist	PtList;

/* Linked-list node for 3-d coordinates.				*/
struct pointlist
{
    float	c_point[3];
    PtList	*c_next;
};


/* Octree node.								*/
struct octree
{
    short	o_bitv;		/* Bit vector identifying this octant.	*/
    short	o_temp;		/* Temperature datum (deg.fahrenheit).	*/
    PtList	*o_points;	/* Origin of octant and member coords.	*/
    Trie	*o_triep;	/* Associated region's trie tree leaf.	*/
    Octree	*o_sibling;	/* Next octant at this level.		*/
    Octree	*o_child;	/* Sub-octants (next level).		*/
};

/* Linked-list of octree nodes used by the trie tree.			*/
struct octreeplist
{
    Octree	*p_octp;
    OcList	*p_next;
};

/* Trie tree node.							*/
union trie
{
    struct	/* Internal nodes: datum is current letter.		*/
    {
	int	t_char;  /* Current letter.			*/
	Trie	*t_altr; /* Alternate letter node link.		*/
	Trie	*t_next; /* Next letter node link.		*/
    }
    n;
    struct	/* Leaf nodes: datum is list of octree leaves.		*/
    {
	OcList	*t_octp; /* Octree leaf list pointer.		*/
	Trie	*t_altr; /* Alternate letter node link.		*/
	Trie	*t_next; /* Next letter node link.		*/
    }
    l;
};

/* Header format for storing data base on the disk.			*/
typedef struct
{
    int	f_temp;
    int	f_length;
}
F_Hdr_Ptlist;

#define PTLIST_NULL	(PtList *) NULL
#define OCTREE_NULL	(Octree *) NULL
#define OCLIST_NULL	(OcList *) NULL
#define TRIE_NULL	(Trie *) NULL

/* Out-of-band temperature for flagging un-initialized octants.		*/
#define ABSOLUTE_ZERO	-459 /* -459.67 degrees fahrenheit.		*/

/* Temperature ranges, set when IR data header is read.			*/
#define AMBIENT		ir_min
#define HOTTEST		ir_max
#define RANGE		(HOTTEST-AMBIENT)

/* "ir_mapping" values, if true it's a bit mask.			*/
#define IR_OFF		0      /* Not in IR module.			*/
#define IR_READONLY	1      /* Read IR data base.			*/
#define IR_EDIT		(1<<1) /* Edit IR data base.			*/
#define IR_OCTREE	(1<<2) /* If ON, display octree, else GED model.*/

extern OcList		*get_Region_Name(char *inbuf, int bufsz, char *msg);
extern Octree		*find_Octant(Octree *parentp, fastf_t *pt, int *levelp);
extern Octree		*add_Region_Octree(Octree *parentp, fastf_t *pt, Trie *triep, int temp, int level);
extern Trie		*add_Trie(const char *name, Trie **triepp);

extern Octree		ir_octree;
extern Trie		*reg_triep;

/*
 * Local Variables:
 * mode: C
 * tab-width: 8
 * indent-tabs-mode: t
 * c-file-style: "stroustrup"
 * End:
 * ex: shiftwidth=4 tabstop=8
 */
