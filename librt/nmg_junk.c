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
