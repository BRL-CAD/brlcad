#define NMG_HIT_LIST	0
#define NMG_MISS_LIST	1
#define NMG_RT_HIT_MAGIC 0x48697400	/* "Hit" */
#define NMG_RT_HIT_SUB_MAGIC 0x48696d00	/* "Him" */
#define NMG_RT_MISS_MAGIC 0x4d697300	/* "Mis" */

#define NMG_HITMISS_SEG_IN 0x696e00	/* "in" */
#define NMG_HITMISS_SEG_OUT 0x6f757400	/* "out" */

struct hitmiss {
	struct rt_list	l;
	struct hit	hit;
	fastf_t		dist_in_plane;	/* distance from plane intersect */
	int		in_out;		/* is this a seg_in or seg_out */
	struct hitmiss	*other;		/* for keeping track of the other
					 * end of the segment when we know
					 * it
					 */
};

struct ray_data {
	struct xray	*rp;
	struct rt_tol	*tol;
	struct hitmiss	**hitmiss;
	vect_t		invdir;
	pointp_t	plane_pt;
	fastf_t		plane_dist;
	fastf_t		ray_dist_to_plane;
	long		*plane_closest;
};

#define GET_HITMISS(_p) { \
	(_p) = (struct hitmiss *)rt_calloc(1, sizeof(struct hitmiss), \
		"GET_HITMISS"); }


#define HIT 1	/* a hit on a face */
#define MISS 0	/* a miss on the face */

