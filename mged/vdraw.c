/*******************************************************************

CMD_VDRAW - edit vector lists and display them as pseudosolids

OPEN COMMAND
vdraw	open			- with no argument, asks if there is
	  			  an open vlist (1 yes, 0 no)

		name		- opens the specified vlist
				  returns 1 if creating new vlist
				          0 if opening an existing vlist
	
EDITING COMMANDS - no return value

vdraw	write	i	c x y z	- write params into i-th vector
		next	c x y z	- write params to end of vector list
		rpp	x y z x y z - write RPP outline at end of vector list

vdraw	insert	i	c x y z	- insert params in front of i-th vector

vdraw	delete 	i		- delete i-th vector
		last		- delete last vector on list
		all		- delete all vectors on list

PARAMETER SETTING COMMAND - no return value
vdraw	params	color		- set the current color with 6 hex digits
				  representing rrggbb
		name		- change the name of the current vlist

QUERY COMMAND
vdraw	read	i		- returns contents of i-th vector "c x y z"
		color		- return the current color in hex
		length		- return number of vectors in list
		name		- return name of current vlist

DISPLAY COMMAND - 
vdraw	send			- send the current vlist to the display
				  returns 0 on success, -1 if the name
				  conflicts with an existing true solid

CURVE COMMANDS
vdraw	vlist	list		- return list of all existing vlists
vdraw	vlist	delete	name	- delete the named vlist

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

author - Carl Nuzman

Example Use -
	vdraw open rays
	vdraw delete all
	foreach partition $ray {
		...stuff...
		vdraw write next 0 $inpt
		vdraw write next 1 $outpt
	}
	vdraw send


********************************************************************/
#include "conf.h"

#include <stdio.h>
#include <math.h>
#include <signal.h>
#include "tcl.h"
#include "tk.h"


#include "machine.h"
#include "bu.h"
#include "vmath.h"
#include "mater.h"
#include "nmg.h"
#include "raytrace.h"
#include "externs.h"
#include "./sedit.h"
#include "./ged.h"
#include "./mged_solid.h"
#include "./mged_dm.h"

#include "../librt/debug.h"	/* XXX */

#ifndef M_SQRT2
#define M_SQRT2		1.41421356237309504880
#endif


#define VDRW_PREFIX		"_VDRW"
#define VDRW_PREFIX_LEN	6
#define VDRW_MAXNAME	31
#define VDRW_DEF_COLOR	0xffff00
#define REV_BU_LIST_FOR(p,structure,hp)	\
	(p)=BU_LIST_LAST(structure,hp);	\
	BU_LIST_NOT_HEAD(p,hp);		\
	(p)=BU_LIST_PLAST(structure,p)

static struct bu_list vdraw_head;
struct rt_curve {
	struct bu_list	l;
	char		name[VDRW_MAXNAME+1]; 	/* name array */
	long		rgb;	/* color */
	struct bu_list	vhd;	/* head of list of vertices */
};


#if 0
/*XXX Not being called. */
int my_final_check(hp)
struct bu_list *hp;
{
	struct rt_vlist *vp;

	for ( BU_LIST_FOR( vp, rt_vlist, hp) ) {
		RT_CK_VLIST( vp );
		printf("num_used = %d\n", vp->nused);
	}
}
#endif

/*
 *			C M D _ V D R A W
 */
int
cmd_vdraw(clientData, interp, argc, argv)
ClientData clientData;
Tcl_Interp *interp;
int argc;
char **argv;
{
	static struct rt_curve *curhead;
	static int initialized = 0;
	struct rt_curve *rcp, *rcp2;
	struct rt_vlist *vp, *cp, *wp;
	int i, index, uind;
	int length;
	long rgb;
	struct bu_vls killstr;
	struct directory *dp;
	char result_string[90]; /* make sure there's room */
	static char temp_name[VDRW_MAXNAME+1];
	char solid_name [VDRW_MAXNAME+VDRW_PREFIX_LEN+1];
	static int real_flag;

	if(argc < 2 || 7 < argc){
	  struct bu_vls vls;

	  bu_vls_init(&vls);
	  bu_vls_printf(&vls, "help vdraw");
	  Tcl_Eval(interp, bu_vls_addr(&vls));
	  bu_vls_free(&vls);
	  return TCL_ERROR;
	}

	if (!initialized){
		if (BU_LIST_UNINITIALIZED( &rt_g.rtg_vlfree ))
			BU_LIST_INIT( &rt_g.rtg_vlfree );
		BU_LIST_INIT( &vdraw_head );
		curhead = (struct rt_curve *) NULL;
		initialized = 1;
	}

	switch ( argv[1][0] ) {
	case 'w': /*write*/
		if (!curhead) {
			Tcl_AppendResult(interp, "vdraw write: no vlist is currently open.", (char *)NULL);
			return TCL_ERROR;
		}
		if (argc < 5){
			Tcl_AppendResult(interp, "vdraw write: not enough args\n", (char *)NULL);
			return TCL_ERROR;
		}
		if (argv[2][0] == 'n') { /* next */
			for (REV_BU_LIST_FOR(vp, rt_vlist, &(curhead->vhd))){
				if (vp->nused > 0){
					break;
				}
			}
			if (BU_LIST_IS_HEAD(vp,&(curhead->vhd))){
				/* we went all the way through */
				vp = BU_LIST_PNEXT(rt_vlist, vp);
				if (BU_LIST_IS_HEAD(vp,&(curhead->vhd))){
					RT_GET_VLIST(vp);
					BU_LIST_INSERT( &(curhead->vhd), &(vp->l) );
				}
			}
			if (vp->nused >= RT_VLIST_CHUNK){
				vp = BU_LIST_PNEXT(rt_vlist, vp);
				if (BU_LIST_IS_HEAD(vp,&(curhead->vhd))){
					RT_GET_VLIST(vp);
					BU_LIST_INSERT(&(curhead->vhd),&(vp->l));
				}
			}
			cp = vp;
			index = vp->nused;
		} else if (argv[2][0] == 'r') { /* rpp_append */
			point_t	minn, maxx;
			if( argc == 5 )  {
				/* vdraw write rpp {1 2 3} {4 5 6} */
				bn_decode_vect( minn, argv[3] );
				bn_decode_vect( maxx, argv[4] );
			} else if( argc == 9 )  {
				VSET( minn, atof(argv[3]), atof(argv[4]), atof(argv[5]) );
				VSET( maxx, atof(argv[6]), atof(argv[7]), atof(argv[8]) );
			} else {
				Tcl_AppendResult(interp,
					 "vdraw write rpp: wrong # args, 'vdraw write rpp min max'\n",
					(char *)NULL);
				return TCL_ERROR;
			}

			/* Draw outline of the RPP */
			bn_vlist_rpp( &(curhead->vhd), minn, maxx );
			return TCL_OK;
		} else if (sscanf(argv[2], "%d", &uind)<1) {
			Tcl_AppendResult(interp, "vdraw: write index not an integer\n", (char *)NULL);
			return TCL_ERROR;
		} else {
			/* uind holds user-specified index */
			/* only allow one past the end */

			for (BU_LIST_FOR(vp, rt_vlist, &(curhead->vhd)) ){
				if (uind < RT_VLIST_CHUNK){
					/* this is the right vlist */
					break;
				}
				if (vp->nused == 0){
					break;
				}
				uind -= vp->nused;
			}

			if (BU_LIST_IS_HEAD(vp,&(curhead->vhd))){
				if (uind > 0){
					Tcl_AppendResult(interp, "vdraw: write out of range\n", (char *)NULL);
					return TCL_ERROR;
				}
				RT_GET_VLIST(vp);
				BU_LIST_INSERT(&(curhead->vhd),&(vp->l));
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
		if( argc == 7 )  {
			cp->pt[index][0] = atof(argv[4]);
			cp->pt[index][1] = atof(argv[5]);
			cp->pt[index][2] = atof(argv[6]);
		} else {
			if( argc != 5 ||
			    bn_decode_vect( cp->pt[index], argv[4] ) != 3 )  {
				Tcl_AppendResult(interp,
					"vdraw write: wrong # args, need either x y z or {x y z}\n", (char *)NULL);
				return TCL_ERROR;
			}
		}
		/* increment counter only if writing onto end */
		if (index == cp->nused)
			cp->nused++;

		return TCL_OK;
	case 'i': /*insert*/
		if (!curhead) {
			Tcl_AppendResult(interp, "vdraw: no vlist is currently open.", (char *)NULL);
			return TCL_ERROR;
		}
		if (argc < 7){
			Tcl_AppendResult(interp, "vdraw: not enough args", (char *)NULL);
			return TCL_ERROR;
		}
		if (sscanf(argv[2], "%d", &uind)<1) {
			Tcl_AppendResult(interp, "vdraw: insert index not an integer\n", (char *)NULL);
			return TCL_ERROR;
		} 

		/* uinds hold user specified index */
		for (BU_LIST_FOR(vp, rt_vlist, &(curhead->vhd)) ){
			if (uind < RT_VLIST_CHUNK){
				/* this is the right vlist */
				break;
			}
			if (vp->nused == 0){
				break;
			}
			uind -= vp->nused;
		}

		if (BU_LIST_IS_HEAD(vp,&(curhead->vhd))){
			if (uind > 0){
				Tcl_AppendResult(interp, "vdraw: insert out of range\n", (char *)NULL);
				return TCL_ERROR;
			}
			RT_GET_VLIST(vp);
			BU_LIST_INSERT(&(curhead->vhd),&(vp->l));
		}
		if (uind > vp->nused) {
			Tcl_AppendResult(interp, "vdraw: insert out of range\n", (char *)NULL);
			return TCL_ERROR;
		}


		cp = vp;
		index = uind;

		vp = BU_LIST_LAST(rt_vlist, &(curhead->vhd));
		vp->nused++;

		while (vp != cp){
			for (i=vp->nused-1; i > 0; i--){
				vp->cmd[i] = vp->cmd[i-1];
				VMOVE(vp->pt[i],vp->pt[i-1]);
			}
			wp = BU_LIST_PLAST(rt_vlist,vp);
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
	case 'd': /*delete*/
		if (!curhead) {
			Tcl_AppendResult(interp, "vdraw: no vlist is currently open.", (char *)NULL);
			return TCL_ERROR;
		}
		if (argc < 3){
			Tcl_AppendResult(interp, "vdraw: not enough args\n", (char *)NULL);
			return TCL_ERROR;
		}
		if (argv[2][0] == 'a') {
			/* delete all */
			for ( BU_LIST_FOR( vp, rt_vlist, &(curhead->vhd)) ){
				vp->nused = 0;
			}
			return TCL_OK;
		} 
		if (argv[2][0] == 'l') {
			/* delete last */
			for ( REV_BU_LIST_FOR( vp, rt_vlist, &(curhead->vhd)) ){
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

		for ( BU_LIST_FOR(vp, rt_vlist, &(curhead->vhd)) ){
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
		
		wp = BU_LIST_PNEXT(rt_vlist, vp);
		while ( BU_LIST_NOT_HEAD(wp, &(curhead->vhd)) ){
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
			wp = BU_LIST_PNEXT(rt_vlist, vp);
		}

		if (vp->nused <= 0) {
			/* this shouldn't happen */
			Tcl_AppendResult(interp, "vdraw: vlist corrupt", (char *)NULL);
			return TCL_ERROR;
		}
		vp->nused--;

		return TCL_OK;
	case 'r': /*read*/
		if (!curhead) {
			Tcl_AppendResult(interp, "vdraw: no vlist is currently open.", (char *)NULL);
			return TCL_ERROR;
		}
		if (argc < 3) {
			Tcl_AppendResult(interp, "vdraw: need index to read\n", (char *)NULL);
			return TCL_ERROR;
		}
		if (argv[2][0] == 'c') {
			/* read color of current solid */
			sprintf(result_string, "%.6lx", curhead->rgb);
			Tcl_AppendResult(interp, result_string, (char *)NULL);
			return TCL_OK;
		}
		if (argv[2][0] == 'n') {
			/*read name of currently open solid*/
			sprintf(result_string, "%.89s", curhead->name);
			Tcl_AppendResult(interp, result_string, (char *)NULL);
			return TCL_OK;
		}
		if (argv[2][0] == 'l') {
			/* return lenght of list */
			length = 0;
			vp = BU_LIST_FIRST(rt_vlist, &(curhead->vhd));
			while ( !BU_LIST_IS_HEAD(vp, &(curhead->vhd)) ) {
				length += vp->nused;
				vp = BU_LIST_PNEXT(rt_vlist, vp);
			}
			sprintf(result_string, "%d", length);
			Tcl_AppendResult(interp, result_string, (char *)NULL);
			return TCL_OK;
		}
		if (sscanf(argv[2], "%d", &uind) < 1) {
			Tcl_AppendResult(interp, "vdraw: read index not an integer\n", (char *)NULL);
			return TCL_ERROR;
		}

		for ( BU_LIST_FOR(vp, rt_vlist, &(curhead->vhd)) ){
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
	case 's': /*send*/
		if (!curhead) {
			Tcl_AppendResult(interp, "vdraw: no vlist is currently open.", (char *)NULL);
			return TCL_ERROR;
		}
		sprintf(solid_name, VDRW_PREFIX);
		strncat(solid_name, curhead->name, VDRW_MAXNAME);
		if (( dp = db_lookup( dbip, solid_name, LOOKUP_QUIET )) == DIR_NULL ) {
			real_flag = 0;
		} else {
			real_flag = (dp->d_addr == RT_DIR_PHONY_ADDR) ? 0 : 1;
		}
		if (real_flag){
			/* solid exists - don't kill */
			Tcl_AppendResult(interp, "-1", (char *)NULL);
			return TCL_OK;
		}
		if( !dbip->dbi_read_only ) {
		  char *av[4];

		  av[0] = "kill";
		  av[1] = "-f";
		  av[2] = solid_name;
		  av[3] = NULL;

		  (void)f_kill(clientData, interp, 3, av);
		}
		index = 0;
		index = invent_solid( solid_name, &(curhead->vhd), curhead->rgb, 1);
		sprintf(result_string,"%d",index);
		/* 0 means OK, -1 means conflict with real solid name */
		Tcl_AppendResult(interp, result_string, (char *)NULL);
		return TCL_OK;
	case 'p':  /* params */
		if (!curhead) {
			Tcl_AppendResult(interp, "vdraw: no vlist is currently open.", (char *)NULL);
			return TCL_ERROR;
		}
		if (argc < 4) {
			Tcl_AppendResult(interp, "vdraw: need params to set\n", (char *)NULL);
			return TCL_ERROR;
		}
		if (argv[2][0] == 'c'){
			if (sscanf(argv[3],"%lx", &rgb)>0)
				curhead->rgb = rgb;
			return TCL_OK;
		}
		if (argv[2][0] == 'n'){
			/* check for conflicts with existing vlists*/
			for ( BU_LIST_FOR( rcp, rt_curve, &vdraw_head) ) {
				if (!strncmp( rcp->name, argv[3], VDRW_MAXNAME)) {
					sprintf(result_string,"vdraw: name %.40s is already in use\n", argv[3]);
					Tcl_AppendResult(interp,result_string,(char *)NULL);
					return TCL_ERROR;
				}
			}
			/* otherwise name not yet used */
			strncpy(curhead->name, argv[3], VDRW_MAXNAME);
			curhead->name[VDRW_MAXNAME] = (char) NULL;
			Tcl_AppendResult(interp,"0",(char *)NULL);
			return TCL_OK;
		}
		break;
	case 'o': /* open */
		if (argc < 3) {
			if (curhead) {
				Tcl_AppendResult(interp, "1", (char *)NULL);
				return TCL_OK;
			} else {
				Tcl_AppendResult(interp, "0", (char *)NULL);
				return TCL_OK;
			}
		}
		strncpy(temp_name, argv[2], VDRW_MAXNAME);
		temp_name[VDRW_MAXNAME] = (char) NULL;
		curhead = (struct rt_curve *) NULL;
		for ( BU_LIST_FOR( rcp, rt_curve, &vdraw_head) ) {
			if (!strncmp( rcp->name, temp_name, VDRW_MAXNAME)) {
				curhead = rcp;
				break;
			}
		}
		if (!curhead) { /* create new entry */
			BU_GETSTRUCT( rcp, rt_curve );
			BU_LIST_APPEND( &vdraw_head, &(rcp->l) );
			strcpy( rcp->name, temp_name);
			rcp->name[VDRW_MAXNAME] = (char) NULL;
			rcp->rgb = VDRW_DEF_COLOR;
			BU_LIST_INIT(&(rcp->vhd));
			RT_GET_VLIST(vp);
			BU_LIST_APPEND( &(rcp->vhd), &(vp->l) );
			curhead = rcp;
			/* 1 means new entry */
			Tcl_AppendResult(interp, "1", (char *)NULL);
			return TCL_OK;
		} else { /* entry already existed */
			if (BU_LIST_IS_EMPTY(&(curhead->vhd))){
				RT_GET_VLIST(vp);
				BU_LIST_APPEND( &(curhead->vhd), &(vp->l) );
			}
			curhead->name[VDRW_MAXNAME] = (char) NULL; /*safety*/
			/* 0 means entry already existed*/
			Tcl_AppendResult(interp, "0", (char *)NULL);
			return TCL_OK;
		}
	case 'v':
		if (argc<3) {
			Tcl_AppendResult(interp,"vdraw: need more args",(char *)NULL);
			return TCL_ERROR;
		}
		switch  (argv[2][0]) {
		case 'l':
			bu_vls_init(&killstr);
			for ( BU_LIST_FOR( rcp, rt_curve, &vdraw_head) ) {
				bu_vls_strcat( &killstr, rcp->name);
				bu_vls_strcat( &killstr, " ");
			}

			Tcl_AppendResult(interp, bu_vls_addr(&killstr), (char *)NULL);
			bu_vls_free(&killstr);
			return TCL_OK;
		case 'd':
			if (argc<4) {
				Tcl_AppendResult(interp,"vdraw: need name of vlist to delete", (char *)NULL);
				return TCL_ERROR;
			}
			rcp2 = (struct rt_curve *)NULL;
			for ( BU_LIST_FOR( rcp, rt_curve, &vdraw_head) ) {
				if (!strncmp(rcp->name,argv[3],VDRW_MAXNAME)){
					rcp2 = rcp;
					break;
				}
			}
			if (!rcp2) {
				sprintf(result_string,"vdraw: vlist %.40s not found", argv[3]);
				Tcl_AppendResult(interp, result_string, (char *)NULL);
				return TCL_ERROR;
			}
			BU_LIST_DEQUEUE(&(rcp2->l));
			if (curhead == rcp2) {
				if ( BU_LIST_IS_EMPTY( &vdraw_head ) ){
					curhead = (struct rt_curve *)NULL;
				} else {
					curhead = BU_LIST_LAST(rt_curve,&vdraw_head);
				}
			}
			RT_FREE_VLIST(&(rcp2->vhd));
			bu_free((genptr_t) rcp2, "rt_curve");
			return TCL_OK;
		default:
			Tcl_AppendResult(interp,"vdraw: unknown option to vdraw vlist", (char *)NULL);
			return TCL_ERROR;
		}
	default:
		Tcl_AppendResult(interp, "vdraw: see vdraw.c for help\n", (char *)NULL);
		return TCL_ERROR;
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

	if(argc < 1 || 1 < argc){
	  struct bu_vls vls;

	  bu_vls_init(&vls);
	  bu_vls_printf(&vls, "help read_center");
	  Tcl_Eval(interp, bu_vls_addr(&vls));
	  bu_vls_free(&vls);
	  return TCL_ERROR;
	}

	MAT_DELTAS_GET_NEG(pos, view_state->vs_toViewcenter);
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

	if(argc < 1 || 1 < argc){
	  struct bu_vls vls;

	  bu_vls_init(&vls);
	  bu_vls_printf(&vls, "help read_scale");
	  Tcl_Eval(interp, bu_vls_addr(&vls));
	  bu_vls_free(&vls);
	  return TCL_ERROR;
	}

	sprintf(result_string,"%.12e", view_state->vs_Viewscale);
	Tcl_AppendResult(interp, result_string, (char *)NULL);
	return TCL_OK;

}

int 
cmd_viewget(clientData, interp, argc, argv)
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

	if(argc < 2 || 2 < argc){
	  struct bu_vls vls;

	  bu_vls_init(&vls);
	  bu_vls_printf(&vls, "helpdevel viewget");
	  Tcl_Eval(interp, bu_vls_addr(&vls));
	  bu_vls_free(&vls);
	  return TCL_ERROR;
	}

	/* center, size, eye, ypr */
	c = argv[1][0];
	switch(	c ) {
	case 'c': 	/*center*/
		MAT_DELTAS_GET_NEG(pos, view_state->vs_toViewcenter);
		sprintf(result_string,"%.12g %.12g %.12g", pos[0], pos[1], pos[2]);
		Tcl_AppendResult(interp, result_string, (char *)NULL);
		return TCL_OK;
	case 's':	/*size*/
		/* don't use base2local, because rt doesn't */
		sprintf(result_string,"%.12g", view_state->vs_Viewscale * 2.0);
		Tcl_AppendResult(interp, result_string, (char *)NULL);
		return TCL_OK;
	case 'e':	/*eye*/
		VSET(temp, 0.0, 0.0, 1.0);
		MAT4X3PNT(pos, view_state->vs_view2model, temp);
		sprintf(result_string,"%.12g %.12g %.12g",pos[0],pos[1],pos[2]);
		Tcl_AppendResult(interp, result_string, (char *)NULL);
		return TCL_OK;
	case 'y':	/*ypr*/
		bn_mat_trn( mymat, view_state->vs_Viewrot);
		anim_v_unpermute(mymat);
		c = anim_mat2ypr(temp, mymat);
		if (c==2) { 
			Tcl_AppendResult(interp, "mat2ypr - matrix is not a rotation matrix", (char *)NULL);
			return TCL_ERROR;
		}
		VSCALE(temp, temp, bn_radtodeg);	
		sprintf(result_string,"%.12g %.12g %.12g",temp[0],temp[1],temp[2]);
		Tcl_AppendResult(interp, result_string, (char *)NULL);
		return TCL_OK;
	case 'a': 	/* aet*/
		bn_mat_trn(mymat,view_state->vs_Viewrot);
		anim_v_unpermute(mymat);
		c = anim_mat2ypr(temp, mymat);
		if (c==2) { 
			Tcl_AppendResult(interp, "mat2ypr - matrix is not a rotation matrix", (char *)NULL);
			return TCL_ERROR;
		}
		VSCALE(temp, temp, bn_radtodeg);	
		if (temp[0] >= 180.0 ) temp[0] -= 180;
		if (temp[0] < 180.0 ) temp[0] += 180;
		temp[1] = -temp[1];
		temp[2] = -temp[2];
		sprintf(result_string,"%.12g %.12g %.12g",temp[0],temp[1],temp[2]);
		Tcl_AppendResult(interp, result_string, (char *)NULL);
		return TCL_OK;
	case 'q':	/*quat*/
		quat_mat2quat(quat,view_state->vs_Viewrot);
		sprintf(result_string,"%.12g %.12g %.12g %.12g", quat[0],quat[1],quat[2],quat[3]);
		Tcl_AppendResult(interp, result_string, (char *)NULL);
		return TCL_OK;
	default:				
		Tcl_AppendResult(interp, 
			"cmd_viewget: invalid argument. Must be one of center,size,eye,ypr.",
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

	if(argc < 3 || MAXARGS < argc){
	  struct bu_vls vls;

	  bu_vls_init(&vls);
	  bu_vls_printf(&vls, "helpdevel viewset");
	  Tcl_Eval(interp, bu_vls_addr(&vls));
	  bu_vls_free(&vls);
	  return TCL_ERROR;
	}

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
			sprintf(result_string,"viewset: Unknown option %.40s.", argv[i]);
			Tcl_AppendResult(interp, result_string, (char *)NULL);
			return TCL_ERROR;
		}
	}

	/* do size set - don't use units (local2base) because rt doesn't */
	if (in_size) {
		if (size < 0.0001) size = 0.0001;
		view_state->vs_Viewscale = size * 0.5;
	}


	/* overrides */
	if (in_center&&in_eye) {
		in_ypr = in_aet = in_quat = 0;
	}

	if (in_quat) {
		quat_quat2mat( view_state->vs_Viewrot, quat);
	} else if (in_ypr) {
		anim_dy_p_r2mat(mymat, ypr[0], ypr[1], ypr[2]);
		anim_v_permute(mymat);
		bn_mat_trn(view_state->vs_Viewrot, mymat);
	} else if (in_aet) {
		anim_dy_p_r2mat(mymat, aet[0]+180.0, -aet[1], -aet[2]);
		anim_v_permute(mymat);
		bn_mat_trn(view_state->vs_Viewrot, mymat);
	} else if (in_center && in_eye) {
		VSUB2( dir, center, eye);
		view_state->vs_Viewscale = MAGNITUDE(dir);
		if (view_state->vs_Viewscale < 0.00005) view_state->vs_Viewscale = 0.00005;
		/* use current eye norm as backup if dir vertical*/
		VSET(norm, -view_state->vs_Viewrot[0], -view_state->vs_Viewrot[1], 0.0);
		anim_dirn2mat(mymat, dir, norm);
		anim_v_permute(mymat);
		bn_mat_trn(view_state->vs_Viewrot, mymat);
	}

	if (in_center) {
		MAT_DELTAS_VEC_NEG( view_state->vs_toViewcenter, center);
	} else if (in_eye) {
		VSET(temp, 0.0, 0.0, view_state->vs_Viewscale);
		bn_mat_trn(mymat, view_state->vs_Viewrot);
		MAT4X3PNT( dir, mymat, temp);
		VSUB2(temp, dir, eye);
		MAT_DELTAS_VEC( view_state->vs_toViewcenter, temp);
	}

	new_mats();

	return TCL_OK;

}
