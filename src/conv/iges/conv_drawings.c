/*                 C O N V _ D R A W I N G S . C
 * BRL-CAD
 *
 * Copyright (c) 1994-2011 United States Government as represented by
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
/** @file iges/conv_drawings.c
 *
 * Routine to convert IGES drawings to wire edges in BRL-CAD NMG
 * structures.
 *
 */

#include "./iges_struct.h"
#include "./iges_extern.h"

struct views_visible
{
    int de;
    int no_of_views;
    int *view_de;
};


static char *default_drawing_name="iges_drawing";

struct bu_list free_hd;

void
Getstrg(str, id)
    char **str;
    char *id;
{
    int i=(-1), length=0, done=0, lencard;
    char num[80];

    if (card[counter] == eof) {
	/* This is an empty field */
	counter++;
	return;
    } else if (card[counter] == eor) /* Up against the end of record */
	return;

    if (card[72] == 'P')
	lencard = PARAMLEN;
    else
	lencard = CARDLEN;

    if (counter > lencard)
	Readrec(++currec);

    if (*id != '\0')
	bu_log("%s", id);

    while (!done) {
	while ((num[++i] = card[counter++]) != 'H' &&
	       counter <= lencard);
	if (counter > lencard)
	    Readrec(++currec);
	if (num[i] == 'H')
	    done = 1;
    }
    num[++i] = '\0';
    length = atoi(num);

    if (length < 1)
	(*str) = NULL;
    else
	(*str) = (char *)bu_malloc(sizeof(char) * length + 1, "Getstrg: str");
    for (i=0; i<length; i++) {
	if (counter > lencard)
	    Readrec(++currec);
	(*str)[i] = card[counter];
	if (*id != '\0')
	    bu_log("%c", card[counter]);
	counter++;
    }
    (*str)[length] = '\0';
    if (*id != '\0')
	bu_log("%c", '\n');

    while (card[counter] != eof && card[counter] != eor) {
	if (counter < lencard)
	    counter++;
	else
	    Readrec(++currec);
    }

    if (card[counter] == eof) {
	counter++;
	if (counter > lencard)
	    Readrec(++ currec);
    }
}


void
Note_to_vlist(entno, vhead)
    int entno;
    struct bu_list *vhead;
{
    int entity_type;
    int nstrings=0;
    int i;

    if (!BU_LIST_IS_INITIALIZED(&free_hd)) BU_LIST_INIT(&free_hd);

    Readrec(dir[entno]->param);
    Readint(&entity_type, "");
    if (entity_type != 212) {
	bu_log("Expected General Note entity data at P%07d, got type %d\n", dir[entno]->param, entity_type);
	return;
    }

    Readint(&nstrings, "");
    for (i=0; i<nstrings; i++) {
	int str_len=0;
	fastf_t width=0.0, height=0.0;
	int font_code=1;
	fastf_t slant_ang;
	fastf_t rot_ang=0.0;
	int mirror=0;
	int internal_rot=0;
	double local_scale;
	char one_char[2];
	point_t loc, tmp;
	char *str = NULL;

	Readint(&str_len, "");
	Readcnv(&width, "");
	Readcnv(&height, "");
	Readint(&font_code, "");	/* not currently used */
	slant_ang = bn_halfpi;
	Readflt(&slant_ang, "");	/* not currently used */
	Readflt(&rot_ang, "");
	Readint(&mirror, "");	/* not currently used */
	Readint(&internal_rot, "");
	Readcnv(&tmp[X], "");
	Readcnv(&tmp[Y], "");
	Readcnv(&tmp[Z], "");
	Getstrg(&str, "");

	/* apply any tranform */
	MAT4X3PNT(loc, *dir[entno]->rot, tmp);


	local_scale = width/str_len;
	if (height < local_scale)
	    local_scale = height;

	if (local_scale < height)
	    loc[Y] += (height - local_scale)/2.0;

	if (local_scale*str_len < width)
	    loc[X] += (width - (local_scale*str_len))/2.0;

	if (internal_rot) {
	    /* vertical text */
	    /* handle vertical text, one character at a time */
	    int j;
	    double tmp_x, tmp_y;
	    double xdel, ydel;

	    xdel = local_scale * sin(rot_ang);
	    ydel = local_scale * cos(rot_ang);

	    tmp_y = loc[Y];
	    tmp_x = loc[X];
	    one_char[1] = '\0';

	    for (j=0; j<str_len; j++) {
		tmp_x += xdel;
		tmp_y -= ydel;
		one_char[0] = str[j];

		bn_vlist_2string(vhead, &free_hd, one_char ,
				 tmp_x, tmp_y, local_scale,
				 (double)(rot_ang*180.0*bn_invpi));
	    }
	} else
	    bn_vlist_2string(vhead, &free_hd, str ,
			     (double)loc[X], (double)loc[Y], local_scale,
			     (double)(rot_ang*180.0*bn_invpi));

	bu_free(str, "Note_to_vlist: str");
    }
}


void
Get_plane(pl, entno)
    plane_t pl;
    int entno;
{
    int entity_type;
    int i;

    Readrec(dir[entno]->param);
    Readint(&entity_type, "");
    if (entity_type != 108) {
	bu_log("Expected Plane entity data at P%07d, got type %d\n", dir[entno]->param, entity_type);
	return;
    }

    for (i=0; i<4; i++)
	Readflt(&pl[i], "");
}


void
Curve_to_vlist(vhead, ptlist)
    struct bu_list *vhead;
    struct ptlist *ptlist;
{
    struct ptlist *ptr;

    if (ptlist == NULL)
	return;

    if (ptlist->next == NULL)
	return;

    ptr = ptlist;

    RT_ADD_VLIST(vhead, ptr->pt, BN_VLIST_LINE_MOVE);

    ptr = ptr->next;
    while (ptr != NULL) {
	RT_ADD_VLIST(vhead, ptr->pt, BN_VLIST_LINE_DRAW);
	ptr = ptr->next;
    }
}


void
Leader_to_vlist(entno, vhead)
    int entno;
    struct bu_list *vhead;
{
    int entity_type;
    int npts, i;
    point_t tmp, tmp2, tmp3, center;
    vect_t v1, v2, v3;
    fastf_t a, b, c;

    Readrec(dir[entno]->param);
    Readint(&entity_type, "");
    if (entity_type != 214) {
	bu_log("Expected Leader (Arrow) entity data at P%07d, got type %d\n", dir[entno]->param, entity_type);
	return;
    }

    Readint(&npts, "");
    Readcnv(&a, "");
    Readcnv(&b, "");
    Readcnv(&v1[2], "");
    Readcnv(&v1[0], "");
    Readcnv(&v1[1], "");
    v2[2] = v1[2];
    Readcnv(&v2[0], "");
    Readcnv(&v2[1], "");
    VMOVE(center, v1);
    if (dir[entno]->form == 5 || dir[entno]->form == 6) {
	/* need to move v1 towards v2 by distance "a" */
	VSUB2(v3, v2, v1);
	VUNITIZE(v3);
	VJOIN1(v1, v1, a, v3);
    }
    MAT4X3PNT(tmp2, *dir[entno]->rot, v1);
    RT_ADD_VLIST(vhead, tmp2, BN_VLIST_LINE_MOVE);
    MAT4X3PNT(tmp, *dir[entno]->rot, v2);
    RT_ADD_VLIST(vhead, tmp, BN_VLIST_LINE_DRAW);

    for (i=1; i<npts; i++) {
	Readcnv(&v3[0], "");
	Readcnv(&v3[1], "");
	MAT4X3PNT(tmp, *dir[entno]->rot, v3);
	RT_ADD_VLIST(vhead, tmp, BN_VLIST_LINE_DRAW);
    }
    switch (dir[entno]->form) {
	default:
	case 1:
	case 2:
	case 3:
	case 11:
	    /* Create unit vector parallel to leader */
	    v3[0] = v2[0] - v1[0];
	    v3[1] = v2[1] - v1[1];
	    v3[2] = 0.0;
	    VUNITIZE(v3);

	    /* Draw one side of arrow head */
	    RT_ADD_VLIST(vhead, tmp2, BN_VLIST_LINE_MOVE);
	    v2[0] = v1[0] + a*v3[0] - b*v3[1];
	    v2[1] = v1[1] + a*v3[1] + b*v3[0];
	    v2[2] = v1[2];
	    MAT4X3PNT(tmp, *dir[entno]->rot, v2);
	    RT_ADD_VLIST(vhead, tmp, BN_VLIST_LINE_DRAW);

	    /* Now draw other side of arrow head */
	    RT_ADD_VLIST(vhead, tmp2, BN_VLIST_LINE_MOVE);
	    v2[0] = v1[0] + a*v3[0] + b*v3[1];
	    v2[1] = v1[1] + a*v3[1] - b*v3[0];
	    MAT4X3PNT(tmp, *dir[entno]->rot, v2);
	    RT_ADD_VLIST(vhead, tmp, BN_VLIST_LINE_DRAW);
	    break;
	case 4:
	    break;
	case 5:
	case 6: {
	    fastf_t delta, cosdel, sindel, rx, ry;

	    delta = bn_pi/10.0;
	    cosdel = cos(delta);
	    sindel = sin(delta);
	    RT_ADD_VLIST(vhead, tmp2, BN_VLIST_LINE_MOVE);
	    VMOVE(tmp, v1);
	    for (i=0; i<20; i++) {
		rx = tmp[X] - center[X];
		ry = tmp[Y] - center[Y];
		tmp[X] = center[X] + rx*cosdel - ry*sindel;
		tmp[Y] = center[Y] + rx*sindel + ry*cosdel;
		MAT4X3PNT(tmp2, *dir[entno]->rot, tmp);
		RT_ADD_VLIST(vhead, tmp2, BN_VLIST_LINE_DRAW);
	    }
	}
	    break;
	case 7:
	case 8:
	    /* Create unit vector parallel to leader */
	    v3[0] = v2[0] - v1[0];
	    v3[1] = v2[1] - v1[1];
	    v3[2] = 0.0;
	    c = sqrt(v3[0]*v3[0] + v3[1]*v3[1]);
	    v3[0] = v3[0]/c;
	    v3[1] = v3[1]/c;
	    /* Create unit vector perp. to leader */
	    v2[0] = v3[1];
	    v2[1] = (-v3[0]);
	    RT_ADD_VLIST(vhead, tmp2, BN_VLIST_LINE_MOVE);
	    tmp[0] = v1[0] + v2[0]*b/2.0;
	    tmp[1] = v1[1] + v2[1]*b/2.0;
	    tmp[2] = v1[2];
	    MAT4X3PNT(tmp3, *dir[entno]->rot, tmp);
	    RT_ADD_VLIST(vhead, tmp3, BN_VLIST_LINE_DRAW);
	    tmp[0] += v3[0]*a;
	    tmp[1] += v3[1]*a;
	    MAT4X3PNT(tmp3, *dir[entno]->rot, tmp);
	    RT_ADD_VLIST(vhead, tmp3, BN_VLIST_LINE_DRAW);
	    tmp[0] -= v2[0]*b;
	    tmp[1] -= v2[1]*b;
	    MAT4X3PNT(tmp3, *dir[entno]->rot, tmp);
	    RT_ADD_VLIST(vhead, tmp3, BN_VLIST_LINE_DRAW);
	    tmp[0] -= v3[0]*a;
	    tmp[1] -= v3[1]*a;
	    MAT4X3PNT(tmp3, *dir[entno]->rot, tmp);
	    RT_ADD_VLIST(vhead, tmp3, BN_VLIST_LINE_DRAW);
	    RT_ADD_VLIST(vhead, tmp2, BN_VLIST_LINE_DRAW);
	    break;
	case 9:
	case 10:
	    /* Create unit vector parallel to leader */
	    v3[0] = v2[0] - v1[0];
	    v3[1] = v2[1] - v1[1];
	    v3[2] = 0.0;
	    VUNITIZE(v3);

	    /* Create unit vector perp. to leader */
	    v2[0] = v3[1];
	    v2[1] = (-v3[0]);

	    tmp[0] = v1[0] + v2[0]*b/2.0 + v3[0]*a/2.0;
	    tmp[1] = v1[1] + v2[1]*b/2.0 + v3[1]*a/2.0;
	    tmp[2] = v1[2];
	    MAT4X3PNT(tmp3, *dir[entno]->rot, tmp);
	    RT_ADD_VLIST(vhead, tmp3, BN_VLIST_LINE_MOVE);
	    tmp[0] -= v3[0]*a + v2[0]*b;
	    tmp[1] -= v3[1]*a + v2[1]*b;
	    MAT4X3PNT(tmp3, *dir[entno]->rot, tmp);
	    RT_ADD_VLIST(vhead, tmp3, BN_VLIST_LINE_DRAW);
	    break;
    }
}


void
Draw_entities(struct model *m, int de_list[], int no_of_des, fastf_t x, fastf_t y, fastf_t local_scale, fastf_t ang, mat_t *xform)
{
    struct bu_list vhead;
    struct bn_vlist *vp;
    struct ptlist *pts, *ptr;
    struct nmgregion *r;
    struct shell *s;
    int npts;
    int entno;
    int i;
    fastf_t sina, cosa;

    NMG_CK_MODEL(m);

    r = nmg_mrsv(m);
    s = BU_LIST_FIRST(shell, &r->s_hd);

    BU_LIST_INIT(&vhead);
    BU_LIST_INIT(&rt_g.rtg_vlfree);

    sina = sin(ang);
    cosa = cos(ang);

    for (entno=0; entno<totentities; entno++) {
	int status;

	status = (dir[entno]->status/10000)%100;

	/* only draw those entities that are independent and belong to this view */
	if (status && status != 2) /* not independent */
	    continue;

	if (dir[entno]->view) {
	    /* this entitiy doesn't always get drawn */
	    int do_entity=0;

	    /* look for its view entity on the list */
	    for (i=0; i<no_of_des; i++) {
		if (dir[entno]->view == de_list[i]) {
		    /* found it, so we do this entity */
		    do_entity = 1;
		    break;
		}
	    }
	    if (!do_entity)
		continue;
	}

	switch (dir[entno]->type) {
	    case 212:   /* "general note" entity (text) */
		Note_to_vlist(entno, &vhead);
		break;
	    case 214:	/* leader (arrow) */
		Leader_to_vlist(entno, &vhead);
		break;
	    default:
		npts = Getcurve(entno, &pts);
		if (npts > 1)
		    Curve_to_vlist(&vhead, pts);

		/* free list of points */
		ptr = pts;
		while (ptr != NULL) {
		    struct ptlist *tmp_ptr;

		    tmp_ptr = ptr->next;
		    bu_free((char *)ptr, "Draw_entities: ptr");
		    ptr = tmp_ptr;;
		}
		break;
	}

	/* rotate, scale, clip, etc, ect, etc... */
	for (BU_LIST_FOR(vp, bn_vlist, &vhead)) {
	    int nused = vp->nused;

	    for (i=0; i<nused; i++) {
		point_t tmp_pt;

		/* Model to view transform */
		if (xform) {
		    MAT4X3PNT(tmp_pt, *xform, vp->pt[i]);
		} else {
		    VMOVE(tmp_pt, vp->pt[i]);
		}

		/* FIXEM: should do clipping here */

		/* project to XY plane */
		if (do_projection)
		    vp->pt[i][Z] = 0.0;

		/* scale, rotate, and translate */
		if (ZERO(ang)) {
		    vp->pt[i][X] = local_scale * tmp_pt[X] + x;
		    vp->pt[i][Y] = local_scale * tmp_pt[Y] + y;
		    if (!do_projection)
			vp->pt[i][Z] = local_scale * tmp_pt[Z];
		} else {
		    vp->pt[i][X] = local_scale*(cosa*tmp_pt[X] - sina*tmp_pt[Y]) + x;
		    vp->pt[i][Y] = local_scale*(sina*tmp_pt[X] + cosa*tmp_pt[Y]) + y;
		    if (!do_projection)
			vp->pt[i][Z] = local_scale * tmp_pt[Z];
		}
	    }
	}

	/* Convert to BRL-CAD wire edges */
	nmg_vlist_to_wire_edges(s, &vhead);
	RT_FREE_VLIST(&vhead);
    }
}


struct views_visible *
Get_views_visible(entno)
    int entno;
{
    int entity_type;
    int no_of_views;
    int no_of_entities;
    int i, j, junk;
    struct views_visible *vv;

    if (dir[entno]->form != 3 && dir[entno]->form != 4) {
	bu_log("Get_views_visible called for wrong form of Associatitivity entity\n");
	return (struct views_visible *)NULL;
    }

    Readrec(dir[entno]->param);
    Readint(&entity_type, "");
    if (entity_type != 402) {
	bu_log("Expected Views Visible entity data at P%07d, got type %d\n", dir[entno]->param, entity_type);
	return (struct views_visible *)NULL;
    }

    Readint(&no_of_views, "");
    Readint(&no_of_entities, "");
    vv = (struct views_visible *)bu_malloc(sizeof(struct views_visible), "Get_views_visible: vv");
    vv->de = entno * 2 + 1;
    vv->no_of_views = no_of_views;
    vv->view_de = (int *)bu_calloc(no_of_views, sizeof(int), "Get_views_visible: vv->view_de");
    for (i=0; i<no_of_views; i++) {
	Readint(&vv->view_de[i], "");
	if (dir[entno]->form == 3)
	    continue;

	/* skip extra stuff in form 4 */
	for (j=0; j<4; j++)
	    Readint(&junk, "");
    }

    return vv;
}


void
Do_view(m, view_vis_list, entno, x, y, ang)
    struct model *m;
    struct bu_ptbl *view_vis_list;
    int entno;
    fastf_t x, y, ang;
{
    int view_de;
    int entity_type;
    struct views_visible *vv;
    int vv_count=0;
    int *de_list;			/* list of possible view field entries for this view */
    int no_of_des;			/* length of above list */
    int view_number;
    fastf_t local_scale=1.0;
    int clip_de[6];
    plane_t clip[6];
    mat_t *xform;
    int i, j;

    view_de = entno * 2 + 1;

    Readrec(dir[entno]->param);
    Readint(&entity_type, "");
    if (entity_type != 410) {
	bu_log("Expected View entity data at P%07d, got type %d\n", dir[entno]->param, entity_type);
	return;
    }

    Readint(&view_number, "\tView number: ");
    Readflt(&local_scale, "");

    for (i=0; i<6; i++) {
	clip_de[i] = 0;
	Readint(&clip_de[i], "");
	clip_de[i] = (clip_de[i] - 1)/2;
    }

    xform = dir[entno]->rot;

    for (i=0; i<6; i++) {
	for (j=0; j<4; j++)
	    clip[i][j] = 0.0;
    }

    for (i=0; i<6; i++) {
	if (clip_de[i])
	    Get_plane(clip[i], clip_de[i]);
	else {
	    clip[i][W] = MAX_FASTF;
	    switch (i) {
		case 0:
		    clip[i][X] = (-1.0);
		    break;
		case 1:
		    clip[i][Y] = 1.0;
		    break;
		case 2:
		    clip[i][X] = 1.0;
		    break;
		case 3:
		    clip[i][Y] = (-1.0);
		    break;
		case 4:
		    clip[i][Z] = (-1.0);
		    break;
		case 5:
		    clip[i][Z] = 1.0;
		    break;
	    }
	}
    }

    for (i=0; i<BU_PTBL_END(view_vis_list); i++) {
	vv = (struct views_visible *)BU_PTBL_GET(view_vis_list, i);
	for (j=0; j<vv->no_of_views; j++) {
	    if (vv->view_de[j] == view_de) {
		vv_count++;
		break;
	    }
	}
    }

    no_of_des = vv_count + 1;
    de_list = (int *)bu_calloc(no_of_des, sizeof(int), "Do_view: de_list");
    de_list[0] = view_de;
    vv_count=0;
    for (i=0; i<BU_PTBL_END(view_vis_list); i++) {
	vv = (struct views_visible *)BU_PTBL_GET(view_vis_list, i);
	for (j=0; j<vv->no_of_views; j++) {
	    if (vv->view_de[j] == view_de) {
		vv_count++;
		de_list[vv_count] = vv->view_de[j];
		break;
	    }
	}
    }

    Draw_entities(m, de_list, no_of_des, x, y, ang, (fastf_t)local_scale, xform);

    bu_free((char *)de_list, "Do_view: de_list");
}


void
Get_drawing(entno, view_vis_list)
    int entno;
    struct bu_ptbl *view_vis_list;
{
    int entity_type;
    int no_of_views;
    int *view_entno;
    int i;
    fastf_t *x, *y, *ang;
    struct wmember headp;

    BU_LIST_INIT(&headp.l);

    Readrec(dir[entno]->param);
    Readint(&entity_type, "");
    if (entity_type != 404) {
	bu_log("Expected Drawing entity data at P%07d, got type %d\n", dir[entno]->param, entity_type);
	return;
    }
    Readint(&no_of_views, "");
    view_entno = (int *)bu_calloc(no_of_views, sizeof(int), "Get_drawing: view_entno");
    x = (fastf_t *)bu_calloc(no_of_views, sizeof(fastf_t), "Get_drawing: x");
    y = (fastf_t *)bu_calloc(no_of_views, sizeof(fastf_t), "Get_drawing: y");
    ang = (fastf_t *)bu_calloc(no_of_views, sizeof(fastf_t), "Get_drawing: ang");
    for (i=0; i<no_of_views; i++) {
	Readint(&view_entno[i], "");
	view_entno[i] = (view_entno[i] - 1)/2;
	Readflt(&x[i], "");
	Readflt(&y[i], "");
	if (dir[i]->form == 1)
	    Readflt(&ang[i], "");
	else
	    ang[i] = 0.0;
    }

    for (i=0; i<no_of_views; i++) {
	struct model *m;
	struct nmgregion *r;
	struct shell *s;

	m = nmg_mm();

	Do_view(m, view_vis_list, view_entno[i], (fastf_t)x[i], (fastf_t)y[i], (fastf_t)ang[i]);

	/* write the view to the BRL-CAD file if the model is not empty */
	NMG_CK_MODEL(m);
	r = BU_LIST_FIRST(nmgregion, &m->r_hd);
	if (BU_LIST_NOT_HEAD(&r->l, &m->r_hd)) {
	    NMG_CK_REGION(r);
	    s = BU_LIST_FIRST(shell, &r->s_hd);
	    if (BU_LIST_NOT_HEAD(&s->l, &r->s_hd)) {
		if (BU_LIST_NON_EMPTY(&s->eu_hd)) {
		    nmg_rebound(m, &tol);
		    mk_nmg(fdout, dir[view_entno[i]]->name, m);
		    (void)mk_addmember(dir[view_entno[i]]->name, &headp.l, NULL, WMOP_UNION);
		}
	    }
	}
    }

    (void)mk_lfcomb(fdout, dir[entno]->name, &headp, 0)

/*	if (no_of_views) {
	bu_free((char *)view_entno, "Get_drawing: view_entno");
	bu_free((char *)x, "Get_drawing: x");
	bu_free((char *)y, "Get_drawing: y");
	bu_free((char *)ang, "Get_drawing: ang");
	}
*/
	}

void
Conv_drawings()
{
    int i;
    int tot_drawings=0;
    int tot_views=0;
    struct views_visible *vv;
    struct model *m;
    struct nmgregion *r;
    struct shell *s;
    struct bu_ptbl view_vis_list;

    bu_log("\n\nConverting drawing entities:\n");

    bu_ptbl_init(&view_vis_list, 64, " &view_vis_list ");

    for (i=0; i<totentities; i++) {
	if (dir[i]->type != 402)
	    continue;

	if (dir[i]->form != 3 && dir[i]->form != 4)
	    continue;

	vv = Get_views_visible(i);
	if (vv)
	    bu_ptbl_ins(&view_vis_list, (long *)vv);

    }

    for (i=0; i<totentities; i++) {
	if (dir[i]->type == 404)
	    tot_drawings++;
    }

    if (tot_drawings) {
	/* Convert each drawing */
	for (i=0; i<totentities; i++) {
	    if (dir[i]->type == 404)
		Get_drawing(i, &view_vis_list);
	}

	/* free views visible list */
	for (i=0; i<BU_PTBL_END(&view_vis_list); i++) {
	    vv = (struct views_visible *)BU_PTBL_GET(&view_vis_list, i);
	    bu_free((char *)vv->view_de, "Conv_drawings: vv->view_de");
	    bu_free((char *)vv, "Conv_drawings: vv");
	}
	bu_ptbl_free(&view_vis_list);
	return;
    }
    bu_log("\nNo drawings entities\n");

    /* no drawing entities, so look for view entities */
    for (i=0; i<totentities; i++) {
	if (dir[i]->type == 410)
	    tot_views++;
    }

    if (tot_views) {
	struct wmember headp;

	BU_LIST_INIT(&headp.l);
	/* Convert each view */
	for (i=0; i<totentities; i++) {
	    if (dir[i]->type == 410) {
		m = nmg_mm();

		Do_view(m, &view_vis_list, i, 0.0, 0.0, 0.0);

		/* write the drawing to the BRL-CAD file if the model is not empty */
		r = BU_LIST_FIRST(nmgregion, &m->r_hd);
		if (BU_LIST_NOT_HEAD(&r->l, &m->r_hd)) {
		    NMG_CK_REGION(r);
		    s = BU_LIST_FIRST(shell, &r->s_hd);
		    if (BU_LIST_NOT_HEAD(&s->l, &r->s_hd)) {
			if (BU_LIST_NON_EMPTY(&s->eu_hd)) {
			    nmg_rebound(m, &tol);
			    mk_nmg(fdout, dir[i]->name, m);
			    (void)mk_addmember(dir[i]->name, &headp.l, NULL, WMOP_UNION);
			}
		    }
		}
	    }
	}
	(void)mk_lfcomb(fdout, default_drawing_name, &headp, 0)

	    /* free views visible list */
	    for (i=0; i<BU_PTBL_END(&view_vis_list); i++) {
		vv = (struct views_visible *)BU_PTBL_GET(&view_vis_list, i);
		bu_free((char *)vv->view_de, "Conv_drawings: vv->view_de");
		bu_free((char *)vv, "Conv_drawings: vv");
	    }
	bu_ptbl_free(&view_vis_list);

	return;
    }
    bu_log("No view entities\n");

    /* no drawings or views, just convert all independent lines, arcs, etc */
    m = nmg_mm();

    Draw_entities(m, (int *)NULL, 0, 0.0, 0.0, 0.0, 1.0, (mat_t *)NULL);

    /* write the drawing to the BRL-CAD file if the model is not empty */
    r = BU_LIST_FIRST(nmgregion, &m->r_hd);
    if (BU_LIST_NOT_HEAD(&r->l, &m->r_hd)) {
	NMG_CK_REGION(r);
	s = BU_LIST_FIRST(shell, &r->s_hd);
	if (BU_LIST_NOT_HEAD(&s->l, &r->s_hd)) {
	    if (BU_LIST_NON_EMPTY(&s->eu_hd)) {
		nmg_rebound(m, &tol);
		mk_nmg(fdout, default_drawing_name, m);
	    }
	}
    }

    /* free views visible list */
    for (i=0; i<BU_PTBL_END(&view_vis_list); i++) {
	vv = (struct views_visible *)BU_PTBL_GET(&view_vis_list, i);
	bu_free((char *)vv->view_de, "Conv_drawings: vv->view_de");
	bu_free((char *)vv, "Conv_drawings: vv");
    }
    bu_ptbl_free(&view_vis_list);
}


/*
 * Local Variables:
 * mode: C
 * tab-width: 8
 * indent-tabs-mode: t
 * c-file-style: "stroustrup"
 * End:
 * ex: shiftwidth=4 tabstop=8
 */
