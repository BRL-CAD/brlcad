/*******************************************************************

CMD_VDRAW - edit, query, and display a vector list as a pseudosolid

OPEN COMMAND
vdraw	open	name		- select the current pseudo-solid
				  returns 1 if creating new pseudo-solid
				          0 if reading an existing solid vlist
					  -1 if solid not opened (see *)


EDITING COMMANDS - no return value

vdraw	write	i	c x y z	- write params into i-th vector
		next	c x y z	- write params to end of vector list

vdraw	insert	i	c x y z	- insert params in front of i-th vector

vdraw	delete 	i		- delete i-th vector
		last		- delete last vector on list
		all		- delete all vectors on list

PARAMETER SETTING COMMAND - no return value
vdraw	params	color		- set the current color with 6 hex digits
				  representing rrggbb
		name		- change the name of the current pseudosolid

QUERY COMMAND
vdraw	read	i		- returns contents of i-th vector "c x y z"
		color		- return the current color in hex
		length		- return number of vectors in list
		name		- return name of current pseudo-solid

DISPLAY COMMAND - 
vdraw	send			- send the current list to the display
				  returns 0 on success, -1 if the name
				  conflicts with an existing true solid

DEBUGGING COMMAND
vdraw	help	1		- returns the number of vlist chunks
		2	i	- prints the nused param of the i-th chunk

vdraw verbose 			-toggle verbose thing

All textual arguments can be replaced by their first letter.
(e.g. "vdraw d a" instead of "vdraw delete all"

In the above listing:
"i" refers to an integer 
"c" is an integer representing one of the following rt_vlist commands:
	 RT_VLIST_LINE_MOVE	0	/ begin new line /
	 RT_VLIST_LINE_DRAW	1	/ draw line /
	 RT_VLIST_POLY_START	2	/ pt[] has surface normal /
	 RT_VLIST_POLY_MOVE	3	/ move to first poly vertex /
	 RT_VLIST_POLY_DRAW	4	/ subsequent poly vertex /
	 RT_VLIST_POLY_END	5	/ last vert (repeats 1st), draw poly /
	 RT_VLIST_POLY_VERTNORM	6	/ per-vertex normal, for interpoloation /

"x y z" refer to floating point values which represent a point or normal
	vector. For commands 0,1,3,4, and 5, they represent a point, while
	for commands 2 and 6 they represent normal vectors

* note - if name is an existing displayed solid, vdraw will read its 
vector list. It can be edited like any list, but can't be sent until
the name is changed.
	open will fail non-destructively (-1) if a solid exists but is 
not displayed.


author - Carl Nuzman
********************************************************************/
#include "conf.h"

#include <math.h>
#include <signal.h>
#include <stdio.h>
#include "tcl.h"
#include "tk.h"


#include "machine.h"
#include "vmath.h"
#include "db.h"
#include "mater.h"
#include "rtstring.h"
#include "raytrace.h"
#include "nmg.h"
#include "externs.h"
#include "./sedit.h"
#include "./ged.h"
#include "./solid.h"
#include "./dm.h"

#include "../librt/debug.h"	/* XXX */

#ifndef M_SQRT2
#define M_SQRT2		1.41421356237309504880
#endif

static struct rt_list head;

#define MAX_NAME	30
#define DEF_COLOR	0xffff00
#define REV_RT_LIST_FOR(p,structure,hp)	\
	(p)=RT_LIST_LAST(structure,hp);	\
	RT_LIST_NOT_HEAD(p,hp);		\
	(p)=RT_LIST_PLAST(structure,p)

int my_final_check(hp)
struct rt_list *hp;
{
	struct rt_vlist *vp;

	for ( RT_LIST_FOR( vp, rt_vlist, hp) ) {
		RT_CK_VLIST( vp );
		printf("num_used = %d\n", vp->nused);
	}
}


int
cmd_vdraw(clientData, interp, argc, argv)
ClientData clientData;
Tcl_Interp *interp;
int argc;
char **argv;
{
	static initialized = 0;
	static int verbose = 0;
	struct rt_vlist *vp, *cp, *wp;
	int i, index, uind, blocks, change;
	int length;
	int found;
	long rgb;
	static long my_rgb;
	struct rt_vls killstr;
	struct solid *sp, *sp2;
	struct directory *dp;
	char result_string[90]; /* make sure there's room */
	static char vdraw_name[MAX_NAME];
	static char temp_name[MAX_NAME];
	static char def_name[] = "_vdraw_sol_";
	static int real_flag;

	if(mged_cmd_arg_check(argc, argv, (struct funtab *)NULL))
	  return TCL_ERROR;

	if (!initialized){
		if (RT_LIST_UNINITIALIZED( &rt_g.rtg_vlfree ))
			RT_LIST_INIT( &rt_g.rtg_vlfree );
		RT_LIST_INIT( &head );
		RT_GET_VLIST(vp);
		RT_LIST_APPEND( &head, &(vp->l) );
		strncpy(vdraw_name, def_name, MAX_NAME);
		my_rgb = DEF_COLOR;
		real_flag = 0;
		initialized = 1;
	}

	/* following code assumes list non-empty */
	if (RT_LIST_IS_EMPTY(&head)){
		RT_GET_VLIST(vp);
		RT_LIST_APPEND( &head, &(vp->l) );
	}

	switch ( argv[1][0] ) {
	case 'w': /*write*/
		if (argc < 7){
			Tcl_AppendResult(interp, "vdraw: not enough args\n", (char *)NULL);
			return TCL_ERROR;
		}
		if (argv[2][0] == 'n') { /* next */
			for (REV_RT_LIST_FOR(vp, rt_vlist, &head)){
				if (vp->nused > 0){
					break;
				}
			}
			if (RT_LIST_IS_HEAD(vp,&head)){
				/* we went all the way through */
				vp = RT_LIST_PNEXT(rt_vlist, vp);
				if (RT_LIST_IS_HEAD(vp,&head)){
					RT_GET_VLIST(vp);
					RT_LIST_INSERT( &head, &(vp->l) );
				}
			}
			if (vp->nused >= RT_VLIST_CHUNK){
				vp = RT_LIST_PNEXT(rt_vlist, vp);
				if (RT_LIST_IS_HEAD(vp,&head)){
					RT_GET_VLIST(vp);
					RT_LIST_INSERT(&head,&(vp->l));
				}
			}
			cp = vp;
			index = vp->nused;
		} else if (sscanf(argv[2], "%d", &uind)<1) {
			Tcl_AppendResult(interp, "vdraw: write index not an integer\n", (char *)NULL);
			return TCL_ERROR;
		} else {
			/* uind holds user-specified index */
			/* only allow one past the end */

			for (RT_LIST_FOR(vp, rt_vlist, &head) ){
				if (uind < RT_VLIST_CHUNK){
					/* this is the right vlist */
					break;
				}
				if (vp->nused == 0){
					break;
				}
				uind -= vp->nused;
			}

			if (RT_LIST_IS_HEAD(vp,&head)){
				if (uind > 0){
					Tcl_AppendResult(interp, "vdraw: write out of range\n", (char *)NULL);
					return TCL_ERROR;
				}
				RT_GET_VLIST(vp);
				RT_LIST_INSERT(&head,&(vp->l));
			}
			if (uind > vp->nused) {
				Tcl_AppendResult(interp, "vdraw: write out of range\n", (char *)NULL);
				return TCL_ERROR;
			}
			cp = vp;
			index = uind;
		}

		if(sscanf(argv[3],"%d",&(cp->cmd[index])) < 1){
			Tcl_AppendResult(interp, "vdraw: cmd not an integer\n", (char *)NULL);
			return TCL_ERROR;
		}
		cp->pt[index][0] = atof(argv[4]);
		cp->pt[index][1] = atof(argv[5]);
		cp->pt[index][2] = atof(argv[6]);
		/* increment counter only if writing onto end */
		if (index == cp->nused)
			cp->nused++;

		return TCL_OK;
		break;
	case 'i': /*insert*/
		if (argc < 7){
			Tcl_AppendResult(interp, "vdraw: not enough args", (char *)NULL);
			return TCL_ERROR;
		}
		if (sscanf(argv[2], "%d", &uind)<1) {
			Tcl_AppendResult(interp, "vdraw: insert index not an integer\n", (char *)NULL);
			return TCL_ERROR;
		} 

		/* uinds hold user specified index */
		for (RT_LIST_FOR(vp, rt_vlist, &head) ){
			if (uind < RT_VLIST_CHUNK){
				/* this is the right vlist */
				break;
			}
			if (vp->nused == 0){
				break;
			}
			uind -= vp->nused;
		}

		if (RT_LIST_IS_HEAD(vp,&head)){
			if (uind > 0){
				Tcl_AppendResult(interp, "vdraw: insert out of range\n", (char *)NULL);
				return TCL_ERROR;
			}
			RT_GET_VLIST(vp);
			RT_LIST_INSERT(&head,&(vp->l));
		}
		if (uind > vp->nused) {
			Tcl_AppendResult(interp, "vdraw: insert out of range\n", (char *)NULL);
			return TCL_ERROR;
		}


		cp = vp;
		index = uind;

		vp = RT_LIST_LAST(rt_vlist, &head);
		vp->nused++;

		while (vp != cp){
			for (i=vp->nused-1; i > 0; i--){
				vp->cmd[i] = vp->cmd[i-1];
				VMOVE(vp->pt[i],vp->pt[i-1]);
			}
			wp = RT_LIST_PLAST(rt_vlist,vp);
			vp->cmd[0] = wp->cmd[RT_VLIST_CHUNK-1];
			VMOVE(vp->pt[0],wp->pt[RT_VLIST_CHUNK-1]);
			vp = wp;
		} 
		
		for ( i=vp->nused-1; i>index; i--){
			vp->cmd[i] = vp->cmd[i-1];
			VMOVE(vp->pt[i],vp->pt[i-1]);
		}
		if(sscanf(argv[3],"%d",&(vp->cmd[index])) < 1){
			Tcl_AppendResult(interp, "vdraw: cmd not an integer\n", (char *)NULL);
			return TCL_ERROR;
		}
		vp->pt[index][0] = atof(argv[4]);
		vp->pt[index][1] = atof(argv[5]);
		vp->pt[index][2] = atof(argv[6]);
		return TCL_OK;
		break;
	case 'd': /*delete*/
		if (argc < 3){
			Tcl_AppendResult(interp, "vdraw: not enough args\n", (char *)NULL);
			return TCL_ERROR;
		}
		if (argv[2][0] == 'a') {
			/* delete all */
			for ( RT_LIST_FOR( vp, rt_vlist, &head) ){
				vp->nused = 0;
			}
			return TCL_OK;
		} 
		if (argv[2][0] == 'l') {
			/* delete last */
			for ( REV_RT_LIST_FOR( vp, rt_vlist, &head) ){
				if (vp->nused > 0){
					vp->nused--;
					break;
				}
			}
			return TCL_OK;
		}
		if (sscanf(argv[2], "%d", &uind)<1) {
			Tcl_AppendResult(interp, "vdraw: delete index not an integer\n", (char *)NULL);
			return TCL_ERROR;
		}  

		for ( RT_LIST_FOR(vp, rt_vlist, &head) ){
			if (uind < RT_VLIST_CHUNK){
				/* this is the right vlist */
				break;
			}
			if ( vp->nused == 0) {
				/* no point going further */
				break;
			}
			uind -= vp->nused;
		}
		
		if (uind >= vp->nused){
			Tcl_AppendResult(interp, "vdraw: delete out of range\n", (char *)NULL);
			return TCL_ERROR;
		}

		for ( i = uind; i < vp->nused - 1; i++) {
			vp->cmd[i] = vp->cmd[i+1];
			VMOVE(vp->pt[i],vp->pt[i+1]);
		}
		
		wp = RT_LIST_PNEXT(rt_vlist, vp);
		while ( RT_LIST_NOT_HEAD(wp, &head) ){
			if (wp->nused == 0) {
				break;
			}

			vp->cmd[RT_VLIST_CHUNK-1] = wp->cmd[0];
			VMOVE(vp->pt[RT_VLIST_CHUNK-1],wp->pt[0]);

			for(i=0; i< wp->nused - 1; i++){
				wp->cmd[i] = wp->cmd[i+1];
				VMOVE(wp->pt[i],wp->pt[i+1]);
			}
			vp = wp;
			wp = RT_LIST_PNEXT(rt_vlist, vp);
		}

		if (vp->nused <= 0) {
			/* this shouldn't happen */
			Tcl_AppendResult(interp, "vdraw: vlist corrupt", (char *)NULL);
			return TCL_ERROR;
		}
		vp->nused--;

		return TCL_OK;
		break;
	case 'r': /*read*/
		if (argc < 3) {
			Tcl_AppendResult(interp, "vdraw: need index to read\n", (char *)NULL);
			return TCL_ERROR;
		}
		if (argv[2][0] == 'c') {
			/* read color of current solid */
			sprintf(result_string, "%lx", my_rgb);
			Tcl_AppendResult(interp, result_string, (char *)NULL);
			return TCL_OK;
		}
		if (argv[2][0] == 'n') {
			/*read name of currently open solid*/
			Tcl_AppendResult(interp, vdraw_name, (char *)NULL);
			return TCL_OK;
		}
		if (argv[2][0] == 'l') {
			/* return lenght of list */
			length = 0;
			vp = RT_LIST_FIRST(rt_vlist, &head);
			while ( !RT_LIST_IS_HEAD(vp, &head) ) {
				length += vp->nused;
				vp = RT_LIST_PNEXT(rt_vlist, vp);
			}
			sprintf(result_string, "%d", length);
			Tcl_AppendResult(interp, result_string, (char *)NULL);
			return TCL_OK;
		}
		if (sscanf(argv[2], "%d", &uind) < 1) {
			Tcl_AppendResult(interp, "vdraw: read index not an integer\n", (char *)NULL);
			return TCL_ERROR;
		}

		for ( RT_LIST_FOR(vp, rt_vlist, &head) ){
			if (uind < RT_VLIST_CHUNK){
				/* this is the right vlist */
				break;
			}
			if ( vp->nused == 0) {
				/* no point going further */
				break;
			}
			uind -= vp->nused;
		}
		
		if (uind >= vp->nused){
			Tcl_AppendResult(interp, "vdraw: read out of range\n", (char *)NULL);
			return TCL_ERROR;
		}

		sprintf(result_string, "%d %.12e %.12e %.12e", 
			vp->cmd[uind], vp->pt[uind][0],
			vp->pt[uind][1],vp->pt[uind][2]);
		Tcl_AppendResult(interp, result_string, (char *)NULL);
		return TCL_OK;
		break;
	case 's': /*send*/
		if (( dp = db_lookup( dbip, vdraw_name, LOOKUP_QUIET )) == DIR_NULL ) {
			real_flag = 0;
		} else {
			real_flag = (dp->d_addr == RT_DIR_PHONY_ADDR) ? 0 : 1;
		}
		if (real_flag){
			/* solid exists - don't kill */
			Tcl_AppendResult(interp, "-1", (char *)NULL);
			return TCL_OK;
		}
		rt_vls_init(&killstr);
		rt_vls_printf( &killstr, "kill -f %s*\n", vdraw_name );
		(void)cmdline( &killstr, FALSE );
		rt_vls_free(&killstr);
		index = 0;
		if (verbose)
			my_final_check(&head);
		index = invent_solid( vdraw_name, &head, my_rgb, 1);
		sprintf(result_string,"%d",index);
		/* 0 means OK, -1 means conflict with real solid name */
		Tcl_AppendResult(interp, result_string, (char *)NULL);
		return TCL_OK;
		break;
	case 'p':  /* params */
		if (argc < 4) {
			Tcl_AppendResult(interp, "vdraw: need params to set\n", (char *)NULL);
			return TCL_ERROR;
		}
		if (argv[2][0] == 'c'){
			if (sscanf(argv[3],"%lx", &rgb)>0)
				my_rgb = rgb;
			return TCL_OK;
		}
		if (argv[2][0] == 'n'){
			strncpy(vdraw_name, argv[3], MAX_NAME);
			return TCL_OK;
		}
		break;
	case 'h': /* help */
		if (argv[2][0]=='1'){
			uind = 0;
			for ( RT_LIST_FOR( vp, rt_vlist, &head)){
				uind++;
			}
			sprintf(result_string, "%d", uind);
			Tcl_AppendResult(interp, result_string, (char *)NULL);
			return TCL_OK;
		}
		if (argv[2][0]=='2'){
			uind = 0;
			sscanf(argv[3], "%d", &index);
			for ( RT_LIST_FOR( vp, rt_vlist, &head)){
				if (index==uind)
					break;
				uind ++;
			}
			sprintf(result_string, "chunk %d nused %d", index, vp->nused);
			Tcl_AppendResult(interp, result_string, (char *)NULL);
			return TCL_OK;
		}
		break;
	case 'o': /* open */
		
		if (argc < 3) {
			strncpy(temp_name, def_name, MAX_NAME);
		} else {
			strncpy(temp_name, argv[2], MAX_NAME);
		}
		if ( (dp = db_lookup( dbip, temp_name, LOOKUP_QUIET))== DIR_NULL ) {
			/* release old list */
			RT_FREE_VLIST( &head);
			/* initialize new list */
			RT_GET_VLIST(vp);
			RT_LIST_INIT( &head );
			RT_LIST_APPEND( &head, &(vp->l) );
			my_rgb = DEF_COLOR;
			/* new pseudo-solid created */
			strncpy(vdraw_name, temp_name, MAX_NAME);
			Tcl_AppendResult(interp, "1", (char *)NULL);
			return TCL_OK;
		} else {
			found = 0;
			FOR_ALL_SOLIDS( sp ) {
				int j;
				for ( j = sp->s_last; j>=0; j--) {
					if (sp->s_path[j] == dp) {
						found = 1;
						break;
					}
				}
				if (found)
					break;
			}
			if (!found) {
				/* solid exists but is not displayed */
				Tcl_AppendResult(interp, "-1", (char *)NULL);
				return TCL_OK;
			}
			/* release old list */
			RT_FREE_VLIST( &head);
			/* get copy of new list */
			rt_vlist_copy( &head   , &(sp->s_vlist));

			my_rgb = ((sp->s_color[0])<<16) |
				  ((sp->s_color[1])<<8) |
				       (sp->s_color[2]);

			/* existing displayed solid used */
			strncpy(vdraw_name, temp_name, MAX_NAME);
			Tcl_AppendResult(interp, "0", (char *)NULL);
			return TCL_OK;
		}
		break;
	case 'v':
		verbose = (verbose + 1)%2;
		break;
	default:
		Tcl_AppendResult(interp, "vdraw: see drawline.c for help\n", (char *)NULL);
		return TCL_ERROR;
		break;
	}
		
	return TCL_OK;
}


int
cmd_read_center(clientData, interp, argc, argv)
ClientData clientData;
Tcl_Interp *interp;
int argc;
char **argv;
{
	char result_string[90];
	point_t pos;

	if(mged_cmd_arg_check(argc, argv, (struct funtab *)NULL))
	  return TCL_ERROR;

	MAT_DELTAS_GET_NEG(pos, toViewcenter);
	sprintf(result_string,"%.12e %.12e %.12e", pos[0], pos[1], pos[2]);
	Tcl_AppendResult(interp, result_string, (char *)NULL);
	return TCL_OK;

}

int
cmd_read_scale(clientData, interp, argc, argv)
ClientData clientData;
Tcl_Interp *interp;
int argc;
char **argv;
{
	char result_string[90];
	point_t pos;

	if(mged_cmd_arg_check(argc, argv, (struct funtab *)NULL))
	  return TCL_ERROR;

	sprintf(result_string,"%.12e", Viewscale);
	Tcl_AppendResult(interp, result_string, (char *)NULL);
	return TCL_OK;

}

int 
cmd_vget(clientData, interp, argc, argv)
ClientData clientData;
Tcl_Interp *interp;
int argc;
char **argv;
{
	char result_string[90];
	point_t pos, temp;
	quat_t quat;
	mat_t mymat;
	char c;

	if(mged_cmd_arg_check(argc, argv, (struct funtab *)NULL))
	  return TCL_ERROR;

	/* center, size, eye, ypr */
	c = argv[1][0];
	switch(	c ) {
	case 'c': 	/*center*/
		MAT_DELTAS_GET_NEG(pos, toViewcenter);
		sprintf(result_string,"%.12g %.12g %.12g", pos[0], pos[1], pos[2]);
		Tcl_AppendResult(interp, result_string, (char *)NULL);
		return TCL_OK;
	case 's':	/*size*/
		/* don't use base2local, because rt doesn't */
		sprintf(result_string,"%.12g", Viewscale * 2.0);
		Tcl_AppendResult(interp, result_string, (char *)NULL);
		return TCL_OK;
	case 'e':	/*eye*/
		VSET(temp, 0, 0, 1);
		MAT4X3PNT(pos, view2model, temp);
		sprintf(result_string,"%.12g %.12g %.12g",pos[0],pos[1],pos[2]);
		Tcl_AppendResult(interp, result_string, (char *)NULL);
		return TCL_OK;
	case 'y':	/*ypr*/
		mat_trn( mymat, Viewrot);
		anim_v_unpermute(mymat);
		c = anim_mat2ypr(temp, mymat);
		if (c==2) { 
			Tcl_AppendResult(interp, "mat2ypr - matrix is not a rotation matrix", (char *)NULL);
			return TCL_ERROR;
		}
		VSCALE(temp, temp, 180.0/M_PI);	
		sprintf(result_string,"%.12g %.12g %.12g",temp[0],temp[1],temp[2]);
		Tcl_AppendResult(interp, result_string, (char *)NULL);
		return TCL_OK;
	case 'a': 	/* aet*/
		mat_trn(mymat,Viewrot);
		anim_v_unpermute(mymat);
		c = anim_mat2ypr(temp, mymat);
		if (c==2) { 
			Tcl_AppendResult(interp, "mat2ypr - matrix is not a rotation matrix", (char *)NULL);
			return TCL_ERROR;
		}
		VSCALE(temp, temp, 180.0/M_PI);	
		if (temp[0] >= 180.0 ) temp[0] -= 180;
		if (temp[0] < 180.0 ) temp[0] += 180;
		temp[1] = -temp[1];
		temp[2] = -temp[2];
		sprintf(result_string,"%.12g %.12g %.12g",temp[0],temp[1],temp[2]);
		Tcl_AppendResult(interp, result_string, (char *)NULL);
		return TCL_OK;
	case 'q':	/*quat*/
		quat_mat2quat(quat,Viewrot);
		sprintf(result_string,"%.12g %.12g %.12g %.12g", quat[0],quat[1],quat[2],quat[3]);
		Tcl_AppendResult(interp, result_string, (char *)NULL);
		return TCL_OK;
	default:				
		Tcl_AppendResult(interp, 
			"cmd_vget: invalid argument. Must be one of center,size,eye,ypr.",
			(char *)NULL);
		return TCL_ERROR;
			
	}
}

int 
cmd_viewset(clientData, interp, argc, argv)
ClientData clientData;
Tcl_Interp *interp;
int argc;
char **argv;
{
	char result_string[90];
	quat_t quat;
	point_t center, eye;
	vect_t ypr, aet;
	fastf_t size;
	int in_quat, in_center, in_eye, in_ypr, in_aet, in_size;
	int i, res;
	vect_t dir, norm, temp;
	mat_t mymat;

	if(mged_cmd_arg_check(argc, argv, (struct funtab *)NULL))
	  return TCL_ERROR;

	in_quat = in_center = in_eye = in_ypr = in_aet = in_size = 0.0;
	i = 1;
	while(i < argc) {
		switch( argv[i][0] ) {
		case 'q':	/* quaternion */
			if (i+4 >= argc) {
				Tcl_AppendResult(interp, "viewset: quat options requires four parameters", (char *)NULL);
				return TCL_ERROR;
			}
			res = sscanf(argv[i+1],"%lf",quat);
			res += sscanf(argv[i+2],"%lf",quat+1);
			res += sscanf(argv[i+3],"%lf",quat+2);
			res += sscanf(argv[i+4],"%lf",quat+3);
			if (res < 4) {
				Tcl_AppendResult(interp, "viewset: quat option requires four parameters", (char *)NULL);
				return TCL_ERROR;
			}
			in_quat = 1;
			i += 5;
			break;
		case 'y':	/* yaw,pitch,roll */
			if (i+3 >= argc) {
				Tcl_AppendResult(interp, "viewset: ypr option requires three parameters", (char *)NULL);
				return TCL_ERROR;
			}
			res = sscanf(argv[i+1],"%lf",ypr);
			res += sscanf(argv[i+2],"%lf",ypr+1);
			res += sscanf(argv[i+3],"%lf",ypr+2);
			if (res < 3) {
				Tcl_AppendResult(interp, "viewset: ypr option requires three parameters", (char *)NULL);
				return TCL_ERROR;
			}
			in_ypr = 1;
			i += 4;
			break;
		case 'a':	/* azimuth,elevation,twist */
			if (i+3 >= argc) {
				Tcl_AppendResult(interp, "viewset: aet option requires three parameters", (char *)NULL);
				return TCL_ERROR;
			}
			res = sscanf(argv[i+1],"%lf",aet);
			res += sscanf(argv[i+2],"%lf",aet+1);
			res += sscanf(argv[i+3],"%lf",aet+2);
			if (res < 3) {
				Tcl_AppendResult(interp, "viewset: aet option requires three parameters", (char *)NULL);
				return TCL_ERROR;
			}
			in_aet = 1;
			i += 4;
			break;
		case 'c':	/* center point */
			if (i+3 >= argc) {
				Tcl_AppendResult(interp, "viewset: center option requires three parameters", (char *)NULL);
				return TCL_ERROR;
			}
			res = sscanf(argv[i+1],"%lf",center);
			res += sscanf(argv[i+2],"%lf",center+1);
			res += sscanf(argv[i+3],"%lf",center+2);
			if (res < 3) {
				Tcl_AppendResult(interp, "viewset: center option requires three parameters", (char *)NULL);
				return TCL_ERROR;
			}
			in_center = 1;
			i += 4;
			break;
		case 'e':	/* eye_point */
			if (i+3 >= argc) {
				Tcl_AppendResult(interp, "viewset: eye option requires three parameters", (char *)NULL);
				return TCL_ERROR;
			}
			res = sscanf(argv[i+1],"%lf",eye);
			res += sscanf(argv[i+2],"%lf",eye+1);
			res += sscanf(argv[i+3],"%lf",eye+2);
			if (res < 3) {
				Tcl_AppendResult(interp, "viewset: eye option requires three parameters", (char *)NULL);
				return TCL_ERROR;
			}
			in_eye = 1;
			i += 4;
			break;
		case 's': 	/* view size */
			if (i+1 >= argc) {
				Tcl_AppendResult(interp, "viewset: size option requires a parameter", (char *)NULL);
				return TCL_ERROR;
			}
			res = sscanf(argv[i+1],"%lf",&size);
			if (res<1) {
				Tcl_AppendResult(interp, "viewset: size option requires a parameter", (char *)NULL);
				return TCL_ERROR;
			}
			in_size = 1;
			i += 2;
			break;
		default:
			sprintf(result_string,"viewset: Unknown option %s.", argv[i]);
			Tcl_AppendResult(interp, result_string, (char *)NULL);
			return TCL_ERROR;
		}
	}

	/* do size set - don't use units (local2base) because rt doesn't */
	if (in_size) {
		if (size < 0.0001) size = 0.0001;
		Viewscale = size * 0.5;
	}


	/* overrides */
	if (in_center&&in_eye) {
		in_ypr = in_aet = in_quat = 0;
	}

	if (in_quat) {
		quat_quat2mat( Viewrot, quat);
	} else if (in_ypr) {
		anim_dy_p_r2mat(mymat, ypr[0], ypr[1], ypr[2]);
		anim_v_permute(mymat);
		mat_trn(Viewrot, mymat);
	} else if (in_aet) {
		anim_dy_p_r2mat(mymat, aet[0]+180.0, -aet[1], -aet[2]);
		anim_v_permute(mymat);
		mat_trn(Viewrot, mymat);
	} else if (in_center && in_eye) {
		VSUB2( dir, center, eye);
		Viewscale = MAGNITUDE(dir);
		if (Viewscale < 0.00005) Viewscale = 0.00005;
		/* use current eye norm as backup if dir vertical*/
		VSET(norm, -Viewrot[0], -Viewrot[1], 0.0);
		anim_dirn2mat(mymat, dir, norm);
		anim_v_permute(mymat);
		mat_trn(Viewrot, mymat);
	}

	if (in_center) {
		MAT_DELTAS_VEC_NEG( toViewcenter, center);
	} else if (in_eye) {
		VSET(temp, 0.0, 0.0, Viewscale);
		mat_trn(mymat, Viewrot);
		MAT4X3PNT( dir, mymat, temp);
		VSUB2(temp, dir, eye);
		MAT_DELTAS_VEC( toViewcenter, temp);
	}

	new_mats();

	return TCL_OK;

}
