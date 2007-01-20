/*                        J O I N T S . H
 * BRL-CAD
 *
 * Copyright (c) 2004-2007 United States Government as represented by
 * the U.S. Army Research Laboratory.
 *
 * This program is free software; you can redistribute it and/or
 * modify it under the terms of the GNU General Public License as
 * published by the Free Software Foundation; either version 2 of the
 * License, or (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful, but
 * WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
 * General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this file; see the file named COPYING for more
 * information.
 */
/** @file joints.h
 * Joint and constrant information.
 *
 * RCSid "$Header$"
 */
/*
 * A joint has to contain all the information to describe how two
 * segments move in relationship to each other.
 *
 * The two segments are defined by the arc given.  The upper arc
 * is the fixed part, the lower part is the "moving" part.  The arc
 * can be completely rooted or a partial description.
 *
 * In the simplest case, a joint moves in only one direction (1 degree of
 * freedom)  For rotations, this can be described completely as a
 * quaternion.  For a slider, this can be described completely as a unit
 * vector.
 *
 * For two degrees of freedom, Two quaternions describe rotation one then
 * rotation two.  (Math: The two quaternions can be added "?" to make
 * a new quaternion that represents the movement around both axis)  For
 * sliders, two unit vectors and a starting point define the plane used.
 *
 * For three degrees of freedom, three quaternions or three unit vectors.
 *
 * The reason we use the three unit vectors rather than the more usual
 * normal and closest point, is that we can then use a simple parametric
 * statement of how to move.  It would be best that the two vectors be
 * at right angles to each other, but that is not required.
 *
 * This use of unit vectors allows for non-axis aligned movements, it allows
 * for non-square and non-cube movement.
 *
 * Each degree of freedom contains both an upper and lower bound as well
 * as the current value of the joint.  If upper < lower then this degree
 * of freedom is not used.
 */

/* NB: The quaternions should (MUST?) have zero twist! */
struct arc {
	struct bu_list	l;
	int		type;
	char		**arc;
	int		arc_last;
	char		**original;
	int		org_last;
};

#define ARC_UNSET	0x0
#define	ARC_PATH	0x1
#define ARC_ARC		0x2
#define ARC_LIST	0x4
#define ARC_BOTH	0x8

struct	rotation {
	quat_t	quat;		/* direction of rotation */
	double	lower;		/* min value in degrees */
	double	upper;		/* max value in degrees */
	double	current;	/* what the joint is currently at in degrees */
	double	accepted;	/* what was the last accepted value of joint */
};

struct direct {
	vect_t	unitvec;
	double	lower;
	double	upper;
	double	current;
	double	last;
	double	accepted;
	int	changed;
};
struct joint {
	struct bu_list	l;
	int		uses;
	char		*name;
	struct arc	path;
	vect_t		location;

	struct rotation	rots[3];
	struct direct	dirs[3];

	struct animate	*anim;
};
#define	MAGIC_JOINT_STRUCT	0x4a4f4900	/* 1246710016 */
struct jointH {
	struct bu_list	l;
	struct joint	*p;
	int		arc_loc;
	int		flag;
};
#define MAGIC_JOINT_HANDLE	0x44330048	/* 1144193096 */
/*	Constraints
 *
 * Standard double linked list stuff.
 * name of the constraint (not required)
 * Set of is a linked list of joint HANDLES!
 */
struct hold_point {
	int	type;			/* SOLID ID + fixed */
	/* everything should reduce to a point at run time. */
	vect_t	point;
	int	vertex_number;

	struct arc arc;		/* Path to object */
	struct db_full_path path;	/* after processing */
	int	flag;
};
#define ID_FIXED	-1

#define	HOLD_PT_GOOD 0x1

struct j_set_desc {
	char	*joint;
	struct arc	path;
	struct arc	exclude;
};
struct hold {
	struct bu_list	l;
	char		*name;
	/* set of joints is defined by joint to grip list of joints */
	struct j_set_desc	j_set;
	char		*joint;
	struct bu_list	j_head;
	struct hold_point	effector;
	struct hold_point	objective;
	double		weight;
	int		priority;
	int		flag;
	double		eval;
};
#define	HOLD_FLAG_TRIED	0x1
/*
 * Objective can be one of:
 *	POINT, LINE, LINE_SEGMENT, PLANE, FACE
 *The objective can be defined in terms of:
 *	GRIPs, JOINTs or FIXED POINTs
 */
#define	MAGIC_HOLD_STRUCT	0x684f4c63	/* 1750027363 */

/*
 * Local Variables:
 * mode: C
 * tab-width: 8
 * c-basic-offset: 4
 * indent-tabs-mode: t
 * End:
 * ex: shiftwidth=4 tabstop=8
 */
