#include <stdio.h>
#include "tkInt.h"
#include "tkPort.h"
#include "tkCanvas.h"

#define NUM_BEZIER_POINTS	50

typedef struct BezierItem {
	Tk_Item header;
	Tk_Outline outline;
	int num_points;
	double *coords;
} BezierItem;

static Tk_CustomOption tagsOption = {
    (Tk_OptionParseProc *) Tk_CanvasTagsParseProc,
    Tk_CanvasTagsPrintProc, (ClientData) NULL
};
static Tk_CustomOption dashOption = {
    (Tk_OptionParseProc *) TkCanvasDashParseProc,
    TkCanvasDashPrintProc, (ClientData) NULL
};
static Tk_CustomOption pixelOption = {
    (Tk_OptionParseProc *) TkPixelParseProc,
    TkPixelPrintProc, (ClientData) NULL
};
static Tk_CustomOption offsetOption = {
    (Tk_OptionParseProc *) TkOffsetParseProc,
    TkOffsetPrintProc,
    (ClientData) (TK_OFFSET_RELATIVE|TK_OFFSET_INDEX)
};
static Tk_CustomOption stateOption = {
    (Tk_OptionParseProc *) TkStateParseProc,
    TkStatePrintProc, (ClientData) 2
};

static double BezierToPoint( Tk_Canvas canvas, Tk_Item *itemPtr, double *pointPtr);
static void TranslateBezier( Tk_Canvas canvas, Tk_Item *itemPtr, double deltaX, double deltaY);
static void ScaleBezier( Tk_Canvas canvas, Tk_Item *itemPtr, double originX, double originY,
			 double scaleX, double scaleY );
int BezierToPostscript( Tcl_Interp *interp, Tk_Canvas canvas, Tk_Item *itemPtr, int prepass);
int BezierToArea( Tk_Canvas canvas, Tk_Item *itemPtr, double *rectPtr);
static void Calc_Bezier( Tk_Canvas canvas, int num_points, double coords[], double t, XPoint *pt );
static void Calc_Bezier_d( Tk_Canvas canvas, int num_points, double coords[], double t, double pt[2] );
static void DisplayBezier( Tk_Canvas canvas, Tk_Item *itemPtr, Display *display, Drawable drawable,
			   int x, int y, int width, int height);
static void DeleteBezier( Tk_Canvas canvas, Tk_Item *itemPtr, Display *display );
int CreateBezier( Tcl_Interp *interp, Tk_Canvas canvas, Tk_Item *itemPtr, int argc, Tcl_Obj *const argv[]);
static int BezierCoords( Tcl_Interp *interp, Tk_Canvas canvas, Tk_Item *itemPtr, int argc, Tcl_Obj *const argv[] );
static void ComputeBezierBbox( Tk_Canvas canvas, BezierItem *bezierPtr );
static int ConfigureBezier( Tcl_Interp *interp, Tk_Canvas canvas, Tk_Item *itemPtr,
			    int argc, Tcl_Obj *const argv[], int flags );

static Tk_ConfigSpec configSpecs[] = {
    {TK_CONFIG_CUSTOM, "-activedash", (char *) NULL, (char *) NULL,
	(char *) NULL, Tk_Offset(BezierItem, outline.activeDash),
	TK_CONFIG_NULL_OK, &dashOption},
    {TK_CONFIG_COLOR, "-activefill", (char *) NULL, (char *) NULL,
	(char *) NULL, Tk_Offset(BezierItem, outline.activeColor),
	TK_CONFIG_NULL_OK},
    {TK_CONFIG_BITMAP, "-activestipple", (char *) NULL, (char *) NULL,
	(char *) NULL, Tk_Offset(BezierItem, outline.activeStipple),
	TK_CONFIG_NULL_OK},
    {TK_CONFIG_CUSTOM, "-activewidth", (char *) NULL, (char *) NULL,
	"0.0", Tk_Offset(BezierItem, outline.activeWidth),
	TK_CONFIG_DONT_SET_DEFAULT, &pixelOption},
    {TK_CONFIG_COLOR, "-fill", (char *) NULL, (char *) NULL,
	"black", Tk_Offset(BezierItem, outline.color), TK_CONFIG_NULL_OK},
    {TK_CONFIG_CUSTOM, "-dash", (char *) NULL, (char *) NULL,
	(char *) NULL, Tk_Offset(BezierItem, outline.dash),
	TK_CONFIG_NULL_OK, &dashOption},
    {TK_CONFIG_PIXELS, "-dashoffset", (char *) NULL, (char *) NULL,
	"0", Tk_Offset(BezierItem, outline.offset),
	TK_CONFIG_DONT_SET_DEFAULT},
    {TK_CONFIG_CUSTOM, "-disableddash", (char *) NULL, (char *) NULL,
	(char *) NULL, Tk_Offset(BezierItem, outline.disabledDash),
	TK_CONFIG_NULL_OK, &dashOption},
    {TK_CONFIG_COLOR, "-disabledfill", (char *) NULL, (char *) NULL,
	(char *) NULL, Tk_Offset(BezierItem, outline.disabledColor),
	TK_CONFIG_NULL_OK},
    {TK_CONFIG_BITMAP, "-disabledstipple", (char *) NULL, (char *) NULL,
	(char *) NULL, Tk_Offset(BezierItem, outline.disabledStipple),
	TK_CONFIG_NULL_OK},
    {TK_CONFIG_CUSTOM, "-disabledwidth", (char *) NULL, (char *) NULL,
	"0.0", Tk_Offset(BezierItem, outline.disabledWidth),
	TK_CONFIG_DONT_SET_DEFAULT, &pixelOption},
    {TK_CONFIG_CUSTOM, "-offset", (char *) NULL, (char *) NULL,
	"0,0", Tk_Offset(BezierItem, outline.tsoffset),
	TK_CONFIG_DONT_SET_DEFAULT, &offsetOption},
    {TK_CONFIG_CUSTOM, "-state", (char *) NULL, (char *) NULL,
	(char *) NULL, Tk_Offset(Tk_Item, state), TK_CONFIG_NULL_OK,
	&stateOption},
    {TK_CONFIG_BITMAP, "-stipple", (char *) NULL, (char *) NULL,
	(char *) NULL, Tk_Offset(BezierItem, outline.stipple),
	TK_CONFIG_NULL_OK},
    {TK_CONFIG_CUSTOM, "-tags", (char *) NULL, (char *) NULL,
	(char *) NULL, 0, TK_CONFIG_NULL_OK, &tagsOption},
    {TK_CONFIG_CUSTOM, "-width", (char *) NULL, (char *) NULL,
	"1.0", Tk_Offset(BezierItem, outline.width),
	TK_CONFIG_DONT_SET_DEFAULT, &pixelOption},
    {TK_CONFIG_END, (char *) NULL, (char *) NULL, (char *) NULL,
	(char *) NULL, 0, 0}
};

Tk_ItemType tkBezierType = {
    "bezier",				/* name */
    sizeof(BezierItem),			/* itemSize */
    CreateBezier,			/* createProc */
    configSpecs,			/* configSpecs */
    ConfigureBezier,			/* configureProc */
    BezierCoords,			/* coordProc */
    DeleteBezier,			/* deleteProc */
    DisplayBezier,			/* displayProc */
    TK_CONFIG_OBJS,			/* flags */
    BezierToPoint,			/* pointProc */
    BezierToArea,			/* areaProc */
    BezierToPostscript,			/* postscriptProc */
    ScaleBezier,			/* scaleProc */
    TranslateBezier,			/* translateProc */
    (Tk_ItemIndexProc *) NULL,		/* indexProc */
    (Tk_ItemCursorProc *) NULL,		/* icursorProc */
    (Tk_ItemSelectionProc *) NULL,	/* selectionProc */
    (Tk_ItemInsertProc *) NULL,		/* insertProc */
    (Tk_ItemDCharsProc *) NULL,		/* dTextProc */
    (Tk_ItemType *) NULL,		/* nextPtr */
};

void
Tk_CreateCanvasBezierType()
{
	Tk_CreateItemType( &tkBezierType );
}

static void
TranslateBezier(
    Tk_Canvas canvas,			/* Canvas containing item. */
    Tk_Item *itemPtr,			/* Item that is being moved. */
    double deltaX, double deltaY)	/* Amount by which item is to be moved. */
{
	BezierItem *bezierPtr = (BezierItem *) itemPtr;
	int i;

	for( i=0 ; i<bezierPtr->num_points ; i++ ) {
		bezierPtr->coords[2*i] += deltaX;
		bezierPtr->coords[2*i+1] += deltaY;
	}
	ComputeBezierBbox(canvas, bezierPtr);
}

static void
ScaleBezier(
    Tk_Canvas canvas,			/* Canvas containing line. */
    Tk_Item *itemPtr,			/* Line to be scaled. */
    double originX, double originY,	/* Origin about which to scale rect. */
    double scaleX,			/* Amount to scale in X direction. */
    double scaleY)			/* Amount to scale in Y direction. */
{
	BezierItem *bezierPtr = (BezierItem *) itemPtr;
	int i;

	for( i=0 ; i<bezierPtr->num_points ; i++ ) {
		bezierPtr->coords[2*i] = originX + scaleX * (bezierPtr->coords[2*i] - originX );
		bezierPtr->coords[2*i+1] = originY + scaleY * (bezierPtr->coords[2*i+1] - originY );
	}
	ComputeBezierBbox(canvas, bezierPtr);
}

int
BezierToPostscript(
    Tcl_Interp *interp,			/* Leave Postscript or error message here. */
    Tk_Canvas canvas,			/* Information about overall canvas. */
    Tk_Item *itemPtr,			/* Item for which Postscript is wanted. */
    int prepass)			/* 1 means this is a prepass to
					 * collect font information;  0 means
					 * final Postscript is being created. */
{
	double t=0.0;
	int i;
	
	double points[2*NUM_BEZIER_POINTS];
	BezierItem *bezierPtr = (BezierItem *) itemPtr;

	/* for now, just calculate a bunch of points */
	for( i = 0; i < NUM_BEZIER_POINTS ;
	     t = 1.0 - (double)(NUM_BEZIER_POINTS-i-1)/((double)(NUM_BEZIER_POINTS - 1)), i++) {
		Calc_Bezier_d( canvas, bezierPtr->num_points, bezierPtr->coords, t, &points[2*i] );
	}

	Tk_CanvasPsPath( interp, canvas, points, NUM_BEZIER_POINTS );

	return TCL_OK;
}

int
BezierToArea(
    Tk_Canvas canvas,		/* Canvas containing item. */
    Tk_Item *itemPtr,		/* Item to check against line. */
    double *rectPtr)
{
	BezierItem *bezierPtr = (BezierItem *) itemPtr;
	double *poly;
	int i;
	int ret;

	poly = (double *)ckalloc( (unsigned)(sizeof(double) * (bezierPtr->num_points + 1) * 2 ) );

	for( i=0 ; i<bezierPtr->num_points*2 ; i++ )
		poly[i] = bezierPtr->coords[i];

	/* close the polygon */
	poly[i++] = bezierPtr->coords[0];
	poly[i++] = bezierPtr->coords[1];

	ret = TkPolygonToArea( poly, bezierPtr->num_points + 1, rectPtr );
	ckfree( (char *)poly );

	return( ret );
}

#define INDEX(_i, _j, _k)	((_i) * (num_points * 2) + (_j) * 2 + (_k))

static void
Calc_Bezier(
	    Tk_Canvas canvas,
	    int num_points,
	    double coords[],
	    double t,
	    XPoint *pt )
{
	double *Vtemp;
	int i, j;
	int degree = num_points - 1;

	Vtemp = (double *)ckalloc( num_points * num_points * 2 * sizeof( double ) );

	for( j=0 ; j<num_points ; j++ ) {
		Vtemp[INDEX(0, j, 0)] = coords[j*2];
		Vtemp[INDEX(0, j, 1)] = coords[j*2+1];
	}

	for( i = 1 ; i < num_points ; i++ ) {
		for( j=0 ; j<= degree - i ; j++ ) {
			Vtemp[INDEX(i, j, 0)] =
				(1.0 - t) * Vtemp[INDEX(i-1, j, 0)] + t * Vtemp[INDEX(i-1, j+1, 0)];
			Vtemp[INDEX(i, j, 1)] =
				(1.0 - t) * Vtemp[INDEX(i-1, j, 1)] + t * Vtemp[INDEX(i-1, j+1, 1)];
		}
	}

	Tk_CanvasDrawableCoords( canvas, Vtemp[INDEX(degree, 0, 0)],
				 Vtemp[INDEX(degree, 0, 1)], &pt->x, &pt->y );

	ckfree( ( char *)Vtemp );
}

static void
Calc_Bezier_d(
	    Tk_Canvas canvas,
	    int num_points,
	    double coords[],
	    double t,
	    double pt[2] )
{
	double *Vtemp;
	int i, j;
	int degree = num_points - 1;

	Vtemp = (double *)ckalloc( degree * degree * 2 * sizeof( double ) );

	for( j=0 ; j<degree ; j++ ) {
		Vtemp[INDEX(0, j, 0)] = coords[j*2];
		Vtemp[INDEX(0, j, 1)] = coords[j*2+1];
	}

	for( i = 1 ; i < degree ; i++ ) {
		for( j=0 ; j<degree ; j++ ) {
			Vtemp[INDEX(i, j, 0)] =
				(1.0 - t) * Vtemp[INDEX(i-1, j, 0)] + t * Vtemp[INDEX(i-1, j+1, 0)];
			Vtemp[INDEX(i, j, 1)] =
				(1.0 - t) * Vtemp[INDEX(i-1, j, 1)] + t * Vtemp[INDEX(i-1, j+1, 1)];
		}
	}

	pt[0] = Vtemp[INDEX(degree-1, 0, 0)];
	pt[1] = Vtemp[INDEX(degree-1, 0, 1)];

	ckfree( (char *)Vtemp );
}

static void
DisplayBezier(
    Tk_Canvas canvas,			/* Canvas that contains item. */
    Tk_Item *itemPtr,			/* Item to be displayed. */
    Display *display,			/* Display on which to draw item. */
    Drawable drawable,			/* Pixmap or window in which to draw item. */
    int x, int y,			/* Describes region of canvas that */
    int width, int height)		/* must be redisplayed (not used). */
{
	double t;
	int i;
	
	XPoint points[NUM_BEZIER_POINTS];
	BezierItem *bezierPtr = (BezierItem *) itemPtr;

	/* for now, just calculate a bunch of points */
	for( i = 0; i < NUM_BEZIER_POINTS ; i++) {
		t = 1.0 - (double)(NUM_BEZIER_POINTS-i-1)/((double)(NUM_BEZIER_POINTS - 1));
		Calc_Bezier( canvas, bezierPtr->num_points, bezierPtr->coords, t, &points[i] );
	}

	Tk_ChangeOutlineGC(canvas, itemPtr, &(bezierPtr->outline));

	XDrawLines(display, drawable, bezierPtr->outline.gc, points,
	       NUM_BEZIER_POINTS, CoordModeOrigin);
}

static void
DeleteBezier(
    Tk_Canvas canvas,			/* Info about overall canvas widget. */
    Tk_Item *itemPtr,			/* Item that is being deleted. */
    Display *display)			/* Display containing window for canvas. */
{
	BezierItem *bezierPtr = (BezierItem *)itemPtr;

	ckfree( (char *)bezierPtr->coords );

	return;
}

int
CreateBezier( 
    Tcl_Interp *interp,			/* Interpreter for error reporting. */
    Tk_Canvas canvas,			/* Canvas to hold new item. */
    Tk_Item *itemPtr,			/* Record to hold new item. */
    int argc,				/* Number of arguments in argv. */
    Tcl_Obj *const argv[])		/* Arguments describing line. */
{
    BezierItem *bezierPtr = (BezierItem *)itemPtr;
    int i;

    Tk_CreateOutline(&(bezierPtr->outline));

    /*
     * Count the number of points and then parse them into a point
     * array.  Leading arguments are assumed to be points if they
     * start with a digit or a minus sign followed by a digit.
     */

    for (i = 0; i < argc; i++) {
	char *arg = Tcl_GetStringFromObj(argv[i], NULL);
	if ((arg[0] == '-') && (arg[1] >= 'a')
		&& (arg[1] <= 'z')) {
	    break;
	}
    }
    bezierPtr->num_points = 0;
    bezierPtr->coords = (double *)NULL;
    if (i && (BezierCoords(interp, canvas, itemPtr, i, argv) != TCL_OK)) {
	goto error;
    }
    if (ConfigureBezier(interp, canvas, itemPtr, argc-i, argv+i, 0) == TCL_OK) {
	return TCL_OK;
    }

    error:
    DeleteBezier(canvas, itemPtr, Tk_Display(Tk_CanvasTkwin(canvas)));
    return TCL_ERROR;
}


static int
BezierCoords(interp, canvas, itemPtr, argc, argv)
    Tcl_Interp *interp;			/* Used for error reporting. */
    Tk_Canvas canvas;			/* Canvas containing item. */
    Tk_Item *itemPtr;			/* Item whose coordinates are to be
					 * read or modified. */
    int argc;				/* Number of coordinates supplied in
					 * argv. */
    Tcl_Obj *const argv[];		/* Array of coordinates: x1, y1,
					 * x2, y2, ... */
{
	BezierItem *bezierPtr = (BezierItem *) itemPtr;
	int i;

	if (argc == 0 ) {
		double *coordPtr;
		Tcl_Obj *subobj, *obj = Tcl_NewObj();

		coordPtr = bezierPtr->coords;
		for( i=0 ; i<bezierPtr->num_points*2 ; i++ ) {
			subobj = Tcl_NewDoubleObj(*coordPtr);
			Tcl_ListObjAppendElement(interp, obj, subobj);
			coordPtr++;
		}
		Tcl_SetObjResult(interp, obj);
		return TCL_OK;
	}

	if (argc == 1) {
		if (Tcl_ListObjGetElements(interp, argv[0], &argc,
					   (Tcl_Obj ***) &argv) != TCL_OK) {
			return TCL_ERROR;
		}
	}

    if (argc & 1) {
	Tcl_AppendResult(interp,
		"odd number of coordinates specified for bezier",
		(char *) NULL);
	return TCL_ERROR;
    } else if (argc < 4) {
	Tcl_AppendResult(interp,
		"incorrect number of coordinates specified for bezier",
		(char *) NULL);
	return TCL_ERROR;
    } else {
	    if( bezierPtr->coords )
		    ckfree( (char *)bezierPtr->coords );
	    bezierPtr->coords = (double *)ckalloc( (unsigned)(argc * 2 * sizeof( double ) ) );
	    bezierPtr->num_points = argc / 2;
	for (i = 0; i <argc; i++) {
	    if (Tk_CanvasGetCoordFromObj(interp, canvas, argv[i],
		    &bezierPtr->coords[i]) != TCL_OK) {
  		return TCL_ERROR;
  	    }
  	}
	ComputeBezierBbox(canvas, bezierPtr);
    }
    return TCL_OK;
}

static void
ComputeBezierBbox(
    Tk_Canvas canvas,			/* Canvas that contains item. */
    BezierItem *bezierPtr )		/* Item whose bbos is to be recomputed. */
{
	int i;

	bezierPtr->header.x1 = bezierPtr->coords[0];
	bezierPtr->header.x2 = bezierPtr->coords[0];
	bezierPtr->header.y1 = bezierPtr->coords[1];
	bezierPtr->header.y2 = bezierPtr->coords[1];

	for( i=1 ; i<bezierPtr->num_points ; i++ ) {
		if( bezierPtr->coords[2*i] < bezierPtr->header.x1 )
			bezierPtr->header.x1 = bezierPtr->coords[2*i];
		if( bezierPtr->coords[2*i] > bezierPtr->header.x2 )
			bezierPtr->header.x2 = bezierPtr->coords[2*i];
		if( bezierPtr->coords[2*i+1] < bezierPtr->header.y1 )
			bezierPtr->header.y1 = bezierPtr->coords[2*i+1];
		if( bezierPtr->coords[2*i+1] > bezierPtr->header.y2 )
			bezierPtr->header.y2 = bezierPtr->coords[2*i+1];
	}
}

static int
ConfigureBezier(interp, canvas, itemPtr, argc, argv, flags)
    Tcl_Interp *interp;		/* Used for error reporting. */
    Tk_Canvas canvas;		/* Canvas containing itemPtr. */
    Tk_Item *itemPtr;		/* Bezier item to reconfigure. */
    int argc;			/* Number of elements in argv.  */
    Tcl_Obj *const argv[];	/* Arguments describing things to configure. */
    int flags;			/* Flags to pass to Tk_ConfigureWidget. */
{
    BezierItem *bezierPtr = (BezierItem *) itemPtr;
    XGCValues gcValues;
    unsigned long mask;
    Tk_Window tkwin;
    Tk_State state;

    tkwin = Tk_CanvasTkwin(canvas);
    if (Tk_ConfigureWidget(interp, tkwin, configSpecs, argc, (CONST char **) argv,
	    (char *) bezierPtr, flags|TK_CONFIG_OBJS) != TCL_OK) {
	return TCL_ERROR;
    }

    /*
     * A few of the options require additional processing, such as
     * graphics contexts.
     */

    state = itemPtr->state;

    if(state == TK_STATE_NULL) {
	state = ((TkCanvas *)canvas)->canvas_state;
    }

    if (bezierPtr->outline.activeWidth > bezierPtr->outline.width ||
	    bezierPtr->outline.activeDash.number != 0 ||
	    bezierPtr->outline.activeColor != NULL ||
	    bezierPtr->outline.activeStipple != None) {
	itemPtr->redraw_flags |= TK_ITEM_STATE_DEPENDANT;
    } else {
	itemPtr->redraw_flags &= ~TK_ITEM_STATE_DEPENDANT;
    }
    mask = Tk_ConfigOutlineGC(&gcValues, canvas, itemPtr,
	    &(bezierPtr->outline));

    if (bezierPtr->outline.gc != None) {
	Tk_FreeGC(Tk_Display(tkwin), bezierPtr->outline.gc);
    }
    bezierPtr->outline.gc = Tk_GetGC(tkwin, mask, &gcValues);
    ComputeBezierBbox(canvas, bezierPtr);

    return TCL_OK;
}

/*
Solving the Nearest Point-on-Curve Problem 
and
A Bezier Curve-Based Root-Finder
by Philip J. Schneider
from "Graphics Gems", Academic Press, 1990
*/

#define SGN(a)		(((a)<0) ? -1 : 1)
#define MIN(a,b)	(((a)<(b))?(a):(b))	
#define MAX(a,b)	(((a)>(b))?(a):(b))	


typedef struct Point2Struct {   /* 2d point */
        double x, y;
        } Point2;
typedef Point2 Vector2;

typedef struct IntPoint2Struct {        /* 2d integer point */
        int x, y;
        } IntPoint2;

typedef struct Matrix3Struct {  /* 3-by-3 matrix */
        double element[3][3];
        } Matrix3;

typedef struct Box2dStruct {            /* 2d box */
        Point2 min, max;
        } Box2;

Vector2 *V2Sub(a, b, c)
Vector2 *a, *b, *c;
{
	c->x = a->x-b->x;  c->y = a->y-b->y;
	return(c);
}

double V2Dot(a, b) 
Vector2 *a, *b; 
{
	return((a->x*b->x)+(a->y*b->y));
}

double V2SquaredLength(a) 
Vector2 *a;
{
	return((a->x * a->x)+(a->y * a->y));
}

/*
 *  Forward declarations
 */
double  NearestPointOnCurve();
static	int	FindRoots();
static	Point2	*ConvertToBezierForm();
static	double	ComputeXIntercept();
static	int	ControlPolygonFlatEnough();
static	int	CrossingCount();
static	Point2	Bezier();
static	Vector2	V2ScaleII();

int		MAXDEPTH = 64;	/*  Maximum depth for recursion */

#define	EPSILON	(ldexp(1.0,-MAXDEPTH-1)) /*Flatness control value */

static double
BezierToPoint(
    Tk_Canvas canvas,		/* Canvas containing item. */
    Tk_Item *itemPtr,		/* Item to check against point. */
    double *pointPtr)		/* Pointer to x and y coordinates. */
{
	Point2 in_pt;
	Point2 *V;
	BezierItem *bezierPtr = (BezierItem *) itemPtr;
	int i;
	double dist;

	V = (Point2 *)ckalloc( bezierPtr->num_points * sizeof( Point2 ) );

	in_pt.x = pointPtr[0];
	in_pt.y = pointPtr[1];

	for( i=0 ; i<bezierPtr->num_points ; i++ ) {
		V[i].x = bezierPtr->coords[2*i];
		V[i].y = bezierPtr->coords[2*i+1];
	}

	dist = NearestPointOnCurve(in_pt, bezierPtr->num_points - 1, V);

	ckfree( (char *)V );

	return( dist );
}


/*
 *  NearestPointOnCurve :
 *  	Compute the parameter value of the point on a Bezier
 *		curve segment closest to some arbtitrary, user-input point.
 *		Return the point on the curve at that parameter value.
 *
 */
double NearestPointOnCurve(P, degree, V)
    Point2 	P;			/* The user-supplied point	  */
    int		degree;			/* Degree of this Bezier */
    Point2 	*V;			/* Control points of Bezier */
{
    Point2	*w;			/* Ctl pts for eqn	*/
    double 	*t_candidate;		/* Possible roots		*/     
    int 	n_solutions;		/* Number of roots found	*/
    double	dist;

    /*  Convert problem to Bezier form	*/
    w = ConvertToBezierForm(P, degree, V);

    t_candidate = (double *)ckalloc( (2*degree +3)*sizeof( double ) );

    /* Find all possible roots of  equation */
    n_solutions = FindRoots(w, 2*degree-1, t_candidate, 0);
    ckfree((char *)w);

    /* Compare distances of P to all candidates, and to t=0, and t=1 */
    {
		double new_dist;
		Point2 	p;
		Vector2  v;
		int		i;

	
	/* Check distance to beginning of curve, where t = 0	*/
		dist = V2SquaredLength(V2Sub(&P, &V[0], &v));

	/* Find distances for candidate points	*/
        for (i = 0; i < n_solutions; i++) {
	    	p = Bezier(V, degree, t_candidate[i],
			(Point2 *)NULL, (Point2 *)NULL);
	    	new_dist = V2SquaredLength(V2Sub(&P, &p, &v));
	    	if (new_dist < dist) {
                	dist = new_dist;
    	    }
        }

	/* Finally, look at distance to end point, where t = 1.0 */
		new_dist = V2SquaredLength(V2Sub(&P, &V[degree], &v));
        	if (new_dist < dist) {
            	dist = new_dist;
        }
    }

    ckfree( (char *)t_candidate );

    /*  Return the smallest distance */
    return( dist );
}

/* calculate factorial n*(n-1)*(n-2)*...(n-r+2)*(n-r+1) */
int factorial( int n, int r )
{
	int ret=1;
	int end = n - r;

	if( n == 0 )
		return( 1 );

	while( n > end ) {
		ret *= n;
		n--;
	}

	return( ret );
}

int comb( int n, int r )
{
	if( r == 0 )
		return( 1 );

	return( factorial(n,r) / factorial(r,r) );
	
}

#define	IND(_i, _j)	((_i)*(degree+1) + (_j))

/*
 *  ConvertToBezierForm :
 *		Given a point and a Bezier curve, generate a
 *		Bezier-format equation whose solution finds the point on the
 *      curve nearest the user-defined point.
 */
static Point2 *ConvertToBezierForm(P, degree, V)
    Point2 	P;			/* The point to find t for	*/
    int		degree;			/* degree of the input Bezier	*/
    Point2 	*V;			/* The control points		*/
{
    int 	i, j, k, m, n, ub, lb;	
    int 	row, column;		/* Table indices		*/
    int		w_degree;		/* degree of resulting eqn	*/
    Vector2 	*c;			/* V(i)'s - P			*/
    Vector2 	*d;			/* V(i+1) - V(i)		*/
    Point2 	*w;			/* Ctl pts of 5th-degree curve  */
    double 	*cdTable;		/* Dot product of c, d		*/
    double 	*z;

    w_degree = 2 * degree - 1;

    c = (Vector2 *)ckalloc( (degree + 1) * sizeof( Vector2 ) );
    d = (Vector2 *)ckalloc( degree * sizeof( Vector2 ) );

    cdTable = (double *)ckalloc( degree * (degree + 1) * sizeof( double ) );
    z = (double *)ckalloc( degree * (degree + 1) * sizeof( double ) );

    n = degree;
    for( i=0 ; i<degree ; i++ ) {
	    for( j=0 ; j<degree+1 ; j++ ) {
		    z[IND(i,j)] = (double)comb(n,j) * (double)comb(n-1,i) /
			    (double)comb(2*n-1,i+j);
	    }
    }

    /*Determine the c's -- these are vectors created by subtracting*/
    /* point P from each of the control points				*/
    for (i = 0; i <= degree; i++) {
		V2Sub(&V[i], &P, &c[i]);
    }
    /* Determine the d's -- these are vectors created by subtracting*/
    /* each control point from the next					*/
    for (i = 0; i <= degree - 1; i++) { 
		d[i] = V2ScaleII(V2Sub(&V[i+1], &V[i], &d[i]), (double)degree);
    }

    /* Create the c,d table -- this is a table of dot products of the */
    /* c's and d's							*/
    for (row = 0; row <= degree - 1; row++) {
		for (column = 0; column <= degree; column++) {
	    	cdTable[IND(row, column)] = V2Dot(&d[row], &c[column]);
		}
    }

    /* Now, apply the z's to the dot products, on the skew diagonal*/
    /* Also, set up the x-values, making these "points"		*/
    w = (Point2 *)ckalloc((unsigned)(w_degree+1) * sizeof(Point2));
    for (i = 0; i <= w_degree; i++) {
		w[i].y = 0.0;
		w[i].x = (double)(i) / (double)w_degree;
    }

    n = degree;
    m = degree-1;
    for (k = 0; k <= n + m; k++) {
		lb = MAX(0, k - m);
		ub = MIN(k, n);
		for (i = lb; i <= ub; i++) {
	    	j = k - i;
	    	w[i+j].y += cdTable[IND(j, i)] * z[IND(j, i)];
		}
    }

    ckfree( (char *)c );
    ckfree( (char *)d );
    ckfree( (char *)cdTable );
    ckfree( (char *)z );

    return (w);
}


/*
 *  FindRoots :
 *	Given an equation in Bernstein-Bezier form, find
 *	all of the roots in the interval [0, 1].  Return the number
 *	of roots found.
 */
static int FindRoots(w, degree, t, depth)
    Point2 	*w;			/* The control points		*/
    int 	degree;		/* The degree of the polynomial	*/
    double 	*t;			/* RETURN candidate t-values	*/
    int 	depth;		/* The depth of the recursion	*/
{  
    int 	i;
    Point2 	*Left,			/* New left and right 		*/
    	  	*Right;			/* control polygons		*/
    int 	left_count,		/* Solution count from		*/
		right_count;		/* children			*/
    double 	*left_t,		/* Solutions from kids		*/
	   	*right_t;

    switch (CrossingCount(w, degree)) {
       	case 0 : {	/* No solutions here	*/
	     return 0;	
	}
	case 1 : {	/* Unique solution	*/
	    /* Stop recursion when the tree is deep enough	*/
	    /* if deep enough, return 1 solution at midpoint 	*/
	    if (depth >= MAXDEPTH) {
			t[0] = (w[0].x + w[degree].x) / 2.0;
			return 1;
	    }
	    if (ControlPolygonFlatEnough(w, degree)) {
			t[0] = ComputeXIntercept(w, degree);
			return 1;
	    }
	    break;
	}
}

    /* Otherwise, solve recursively after	*/
    /* subdividing control polygon		*/
    Left = (Point2 *)ckalloc( (degree + 2) * sizeof( Point2 ) );
    Right = (Point2 *)ckalloc( (degree + 2) * sizeof( Point2 ) );
    left_t = (double *)ckalloc( (degree + 3 ) * sizeof( double ) );
    right_t = (double *)ckalloc( (degree + 3 ) * sizeof( double ) );

    Bezier(w, degree, 0.5, Left, Right);
    left_count  = FindRoots(Left,  degree, left_t, depth+1);
    right_count = FindRoots(Right, degree, right_t, depth+1);

    /* Gather solutions together	*/
    for (i = 0; i < left_count; i++) {
        t[i] = left_t[i];
    }
    for (i = 0; i < right_count; i++) {
 		t[i+left_count] = right_t[i];
    }

    ckfree( (char *)Left );
    ckfree( (char *)Right );
    ckfree( (char *)left_t );
    ckfree( (char *)right_t );

    /* Send back total number of solutions	*/
    return (left_count+right_count);
}


/*
 * CrossingCount :
 *	Count the number of times a Bezier control polygon 
 *	crosses the 0-axis. This number is >= the number of roots.
 *
 */
static int CrossingCount(V, degree)
    Point2	*V;			/*  Control pts of Bezier curve	*/
    int		degree;			/*  Degreee of Bezier curve 	*/
{
    int 	i;	
    int 	n_crossings = 0;	/*  Number of zero-crossings	*/
    int		sign, old_sign;		/*  Sign of coefficients	*/

    sign = old_sign = SGN(V[0].y);
    for (i = 1; i <= degree; i++) {
		sign = SGN(V[i].y);
		if (sign != old_sign) n_crossings++;
		old_sign = sign;
    }
    return n_crossings;
}



/*
 *  ControlPolygonFlatEnough :
 *	Check if the control polygon of a Bezier curve is flat enough
 *	for recursive subdivision to bottom out.
 *
 */
static int ControlPolygonFlatEnough(V, degree)
    Point2	*V;		/* Control points	*/
    int 	degree;		/* Degree of polynomial	*/
{
    int 	i;			/* Index variable		*/
    double 	*distance;		/* Distances from pts to line	*/
    double 	max_distance_above;	/* maximum of these		*/
    double 	max_distance_below;
    double 	error;			/* Precision of root		*/
    double 	intercept_1,
    	   	intercept_2,
	   		left_intercept,
		   	right_intercept;
    double 	a, b, c;		/* Coefficients of implicit	*/
    					/* eqn for line from V[0]-V[deg]*/

    /* Find the  perpendicular distance		*/
    /* from each interior control point to 	*/
    /* line connecting V[0] and V[degree]	*/
    distance = (double *)ckalloc((unsigned)(degree + 1) * 					sizeof(double));
    {
	double	abSquared;

	/* Derive the implicit equation for line connecting first and last control points */
	a = V[0].y - V[degree].y;
	b = V[degree].x - V[0].x;
	c = V[0].x * V[degree].y - V[degree].x * V[0].y;

	abSquared = (a * a) + (b * b);

        for (i = 1; i < degree; i++) {
	    /* Compute distance from each of the points to that line	*/
	    	distance[i] = a * V[i].x + b * V[i].y + c;
	    	if (distance[i] > 0.0) {
				distance[i] = (distance[i] * distance[i]) / abSquared;
	    	}
	    	if (distance[i] < 0.0) {
				distance[i] = -((distance[i] * distance[i]) / 						abSquared);
	    	}
		}
    }


    /* Find the largest distance	*/
    max_distance_above = 0.0;
    max_distance_below = 0.0;
    for (i = 1; i < degree; i++) {
		if (distance[i] < 0.0) {
	    	max_distance_below = MIN(max_distance_below, distance[i]);
		};
		if (distance[i] > 0.0) {
	    	max_distance_above = MAX(max_distance_above, distance[i]);
		}
    }
    ckfree((char *)distance);

    {
	double	det, dInv;
	double	a1, b1, c1, a2, b2, c2;

	/*  Implicit equation for zero line */
	a1 = 0.0;
	b1 = 1.0;
	c1 = 0.0;

	/*  Implicit equation for "above" line */
	a2 = a;
	b2 = b;
	c2 = c + max_distance_above;

	det = a1 * b2 - a2 * b1;
	dInv = 1.0/det;
	
	intercept_1 = (b1 * c2 - b2 * c1) * dInv;

	/*  Implicit equation for "below" line */
	a2 = a;
	b2 = b;
	c2 = c + max_distance_below;
	
	det = a1 * b2 - a2 * b1;
	dInv = 1.0/det;
	
	intercept_2 = (b1 * c2 - b2 * c1) * dInv;
    }

    /* Compute intercepts of bounding box	*/
    left_intercept = MIN(intercept_1, intercept_2);
    right_intercept = MAX(intercept_1, intercept_2);

    error = 0.5 * (right_intercept-left_intercept);    
    if (error < EPSILON) {
		return 1;
    }
    else {
		return 0;
    }
}



/*
 *  ComputeXIntercept :
 *	Compute intersection of chord from first control point to last
 *  	with 0-axis.
 * 
 */
/* NOTE: "T" and "Y" do not have to be computed, and there are many useless
 * operations in the following (e.g. "0.0 - 0.0").
 */
static double ComputeXIntercept(V, degree)
    Point2 	*V;			/*  Control points	*/
    int		degree; 		/*  Degree of curve	*/
{
    double	XLK, YLK, XNM, YNM, XMK, YMK;
    double	det, detInv;
    double	S;
    double	X;

    XLK = 1.0 - 0.0;
    YLK = 0.0 - 0.0;
    XNM = V[degree].x - V[0].x;
    YNM = V[degree].y - V[0].y;
    XMK = V[0].x - 0.0;
    YMK = V[0].y - 0.0;

    det = XNM*YLK - YNM*XLK;
    detInv = 1.0/det;

    S = (XNM*YMK - YNM*XMK) * detInv;
/*  T = (XLK*YMK - YLK*XMK) * detInv; */

    X = 0.0 + XLK * S;
/*  Y = 0.0 + YLK * S; */

    return X;
}


/*
 *  Bezier : 
 *	Evaluate a Bezier curve at a particular parameter value
 *      Fill in control points for resulting sub-curves if "Left" and
 *	"Right" are non-null.
 * 
 */
static Point2 Bezier(V, degree, t, Left, Right)
    int 	degree;		/* Degree of bezier curve	*/
    Point2 	*V;			/* Control pts			*/
    double 	t;			/* Parameter value		*/
    Point2 	*Left;		/* RETURN left half ctl pts	*/
    Point2 	*Right;		/* RETURN right half ctl pts	*/
{
    int 	i, j;		/* Index variables	*/
    Point2 	*Vtemp;
    Point2	ret_pt;

    Vtemp = (Point2 *)ckalloc( (degree + 1) * (degree + 1) * sizeof( Point2 ) );

    /* Copy control points	*/
    for (j =0; j <= degree; j++) {
		Vtemp[j] = V[j];
    }

    /* Triangle computation	*/
    for (i = 1; i <= degree; i++) {	
		for (j =0 ; j <= degree - i; j++) {
	    	Vtemp[i*(degree+1) + j].x =
	      		(1.0 - t) * Vtemp[(i-1)*(degree+1) + j].x + t * Vtemp[(i-1)*(degree+1) + j+1].x;
	    	Vtemp[i*(degree+1) + j].y =
	      		(1.0 - t) * Vtemp[(i-1)*(degree+1) + j].y + t * Vtemp[(i-1)*(degree+1) + j+1].y;
		}
    }
    
    if (Left != NULL) {
		for (j = 0; j <= degree; j++) {
	    	Left[j]  = Vtemp[j*(degree+1)];
		}
    }
    if (Right != NULL) {
		for (j = 0; j <= degree; j++) {
	    	Right[j] = Vtemp[(degree-j)*(degree+1) + j];
		}
    }

    ret_pt = Vtemp[degree*(degree+1)];
    ckfree( (char *) Vtemp );

    return (ret_pt);
}

static Vector2 V2ScaleII(v, s)
    Vector2	*v;
    double	s;
{
    Vector2 result;

    result.x = v->x * s; result.y = v->y * s;
    return (result);
}
