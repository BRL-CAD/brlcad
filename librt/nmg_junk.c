/*
 *			N M G _ J U N K . C
 *
 *  This module is a resting place for unfinished subroutines
 *  that are NOT a part of the current NMG library, but which
 *  were sufficiently far along as to be worth saving.
 */



#if 0
/*XXX	Group sets of loops within a face into overlapping units.
 *	This will allow us to separate dis-associated groups into separate
 *	(but equal ;-) faces
 */

struct groupie {
	struct rt_list	l;
	struct loopuse *lu;
}

struct loopgroup {
	struct rt_list	l;
	struct rt_list	groupies;
} groups;


struct loopgroup *
group(lu, groups)
struct loopuse *lu;
struct rt_list *groups;
{
	struct loopgroup *group;
	struct groupie *groupie;

	for (RT_LIST_FOR(group, loopgroup, groups)) {
		for (RT_LIST_FOR(groupie, groupie, &groupies)) {
			if (groupie->lu == lu)
				return(group);
		}
	}
	return NULL;
}

void
new_loop_group(lu, groups)
struct loopuse *lu;
struct rt_list *groups;
{
	struct loopgroup *lg;
	struct groupie *groupie;

	lg = (struct loopgroup *)rt_calloc(sizeof(struct loopgroup), "loopgroup");
	groupie = (struct groupie *)rt_calloc(sizeof(struct groupie), "groupie");
	groupie->lu = lu;

	RT_LIST_INIT(&lg->groupies);
	RT_LIST_APPEND(&lg->groupies, &groupie->l);
	RT_LIST_APPEND(groups, &lg.l);
}

void
merge_groups(lg1, lg2);
struct loopgroup *lg1, *lg2;
{
	struct groupie *groupie;

	while (RT_LIST_WHILE(groupie, groupie, &lg2->groupies)) {
		RT_LIST_DEQUEUE(&(groupie->l));
		RT_LIST_APPEND(&(lg1->groupies), &(groupie->l))
	}
	RT_DEQUEUE(&(lg2->l));
	rt_free((char *)lg2, "free loopgroup 2 of merge");
}
void
free_groups(head)
struct rt_list *head;
{
	while
}

	RT_LIST_INIT(&groups);

	for (RT_LIST_FOR(lu, loopuse, &fu->lu_hd)) {

		/* build loops out of exterior loops only */
		if (lu->orientation == OT_OPPOSITE)
			continue;

		if (group(lu) == NULL)
			new_loop_group(lu, &groups);

		for (RT_LIST_FOR(lu2, loopuse, &fu->lu_hd)) {
			if (lu == lu2 ||
			    group(lu, &groups) == group(lu2, &groups))
				continue;
			if (loops_touch_or_overlap(lu, lu2))
				merge_groups(group(lu), group(lu2));
		}



	}
#endif

#if 0
/*			N M G _ E U _ S Q
 *
 *	squeeze an edgeuse out of a list
 *
 *	All uses of the edge being "Squeezed" must be followed by
 *	the same "next" edge
 *
 */
nmg_eu_sq(eu)
struct edgeuse *eu;
{
	struct edgeuse *matenext;
	NMG_CK_EDGEUSE(eu);
	NMG_CK_EDGEUSE(eu->eumate_p);

	/* foreach use of this edge, there must be exactly one use of the
	 * previous edge.  There may not be any "extra" uses of the
	 * previous edge
	 */



	matenext = RT_LIST_PNEXT_CIRC(eu->eumate_p);
	NMG_CK_EDGEUSE(matenext);

	RT_LIST_DEQUEUE(eu);
	RT_LIST_DEQUEUE(matenext);

}
#endif

/*
 *			N M G _ P O L Y T O N M G
 *
 *	Read a polygon file and convert it to an NMG shell
 *
 *	A polygon file consists of the following:
 *	The first line consists of two integer numbers: the number of points
 *	(vertices) in the file, followed by the number of polygons in the file.
 *	This line is followed by lines for each of the
 *	verticies.  Each vertex is listed on its own line, as the 3tuple
 *	"X Y Z".  After the list of verticies comes the list of polygons.  
 *	each polygon is represented by a line containing 1) the number of
 *	verticies in the polygon, followed by 2) the indicies of the verticies
 *	that make up the polygon.
 *
 *	Implicit return:
 *		r->s_p	A new shell containing all the faces from the
 *			polygon file
 *
 *  XXX This is a horrible way to do this.  Lee violates his own rules
 *  about not creating fundamental structures on his own...  :-)
 *  Retired in favor of more modern tessellation strategies.
 *  For a better example, look in cad/jack/jack-g.c
 */
struct shell *
nmg_polytonmg(fp, r, tol)
FILE *fp;
struct nmgregion	*r;
CONST struct rt_tol	*tol;
{
	int i, j, num_pts, num_facets, pts_this_face, facet;
	int vl_len;
	struct vertex **v;		/* list of all vertices */
	struct vertex **vl;	/* list of vertices for this polygon*/
	point_t p;
	struct shell *s;
	struct faceuse *fu;
	struct loopuse	*lu;
	struct edgeuse *eu;
	plane_t plane;
	struct model	*m;

	s = nmg_msv(r);
	m = s->r_p->m_p;
	nmg_kvu(s->vu_p);

	/* get number of points & number of facets in file */
	if (fscanf(fp, "%d %d", &num_pts, &num_facets) != 2)
		rt_bomb("polytonmg() Error in first line of poly file\n");
	else
		if (rt_g.NMG_debug & DEBUG_POLYTO)
			rt_log("points: %d  facets: %d\n",
				num_pts, num_facets);


	v = (struct vertex **) rt_calloc(num_pts, sizeof (struct vertex *),
			"vertices");

	/* build the vertices */ 
	for (i = 0 ; i < num_pts ; ++i) {
		GET_VERTEX(v[i], m);
		v[i]->magic = NMG_VERTEX_MAGIC;
	}

	/* read in the coordinates of the vertices */
	for (i=0 ; i < num_pts ; ++i) {
		if (fscanf(fp, "%lg %lg %lg", &p[0], &p[1], &p[2]) != 3)
			rt_bomb("polytonmg() Error reading point");
		else
			if (rt_g.NMG_debug & DEBUG_POLYTO)
				rt_log("read vertex #%d (%g %g %g)\n",
					i, p[0], p[1], p[2]);

		nmg_vertex_gv(v[i], p);
	}

	vl = (struct vertex **)rt_calloc(vl_len=8, sizeof (struct vertex *),
		"vertex parameter list");

	for (facet = 0 ; facet < num_facets ; ++facet) {
		if (fscanf(fp, "%d", &pts_this_face) != 1)
			rt_bomb("polytonmg() error getting pt count for this face");

		if (rt_g.NMG_debug & DEBUG_POLYTO)
			rt_log("facet %d pts in face %d\n",
				facet, pts_this_face);

		if (pts_this_face > vl_len) {
			while (vl_len < pts_this_face) vl_len *= 2;
			vl = (struct vertex **)rt_realloc( (char *)vl,
				vl_len*sizeof(struct vertex *),
				"vertex parameter list (realloc)");
		}

		for (i=0 ; i < pts_this_face ; ++i) {
			if (fscanf(fp, "%d", &j) != 1)
				rt_bomb("polytonmg() error getting point index for v in f");
			vl[i] = v[j-1];
		}

		fu = nmg_cface(s, vl, pts_this_face);
		lu = RT_LIST_FIRST( loopuse, &fu->lu_hd );
		/* XXX should check for vertex-loop */
		eu = RT_LIST_FIRST( edgeuse, &lu->down_hd );
		NMG_CK_EDGEUSE(eu);
		if (rt_mk_plane_3pts(plane, eu->vu_p->v_p->vg_p->coord,
		    RT_LIST_PNEXT(edgeuse,eu)->vu_p->v_p->vg_p->coord,
		    RT_LIST_PLAST(edgeuse,eu)->vu_p->v_p->vg_p->coord,
		    tol ) )  {
			rt_log("At %d in %s\n", __LINE__, __FILE__);
			rt_bomb("polytonmg() cannot make plane equation\n");
		}
		else nmg_face_g(fu, plane);
	}

	for (i=0 ; i < num_pts ; ++i) {
		if( RT_LIST_IS_EMPTY( &v[i]->vu_hd ) )  continue;
		FREE_VERTEX(v[i]);
	}
	rt_free( (char *)v, "vertex array");
	return(s);
}
