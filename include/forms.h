/************************************************************************
 *									*
 *  Forms is a library for building up dialogue and interaction boxes.  *
 *  It is based of the Silicon Graphics Graphical Library.		*
 *									*
 *  This software is public domain. It may not be resold.		*
 *									*
 *  Written by: Mark Overmars						*
 *              Department of Computer Science				*
 *              University of Utrecht					*
 *		P.O. Box 80.089						*
 *              3508 TB Utrecht, the Netherlands			*
 *		Email: markov@cs.ruu.nl					*
 *									*
 *  Version 1.1	d							*
 *  Date:  28 Jan 1991							*
 ************************************************************************/

/************   The type FORM              ************/

#define AUTOMAX		25

typedef struct forms {
   int window;          /* Window of the form */
   float w,h;           /* The size of the form */

   int active;		/* Whether active */
   int visible;         /* Whether being displayed */

   int doublebuf;	/* Whether in double buffer mode */

   struct objs *first;	/* First object in the form */
   struct objs *last;	/* Last object in the form */

   struct objs *focusobj; /* Object to which input is directed */
   struct objs *autoobj[AUTOMAX]; /* Automatic objects in the form */
   int autonumb;	/* Their number */
} FORM;

/************   The type OBJECT            ************/

#define MAXSTR			64

typedef struct objs {
   int class;           /* What type of object */
   int type;            /* The type within the class */
   int boxtype;         /* The type of the bounding box */
   float x,y,w,h;       /* The bounding box */
   int col1,col2;       /* Two possible colors */

   char label[MAXSTR];	/* The label */
   int align;		/* Label or text alignment */
   int lcol;		/* Color of the label */
   float lsize; 	/* Size of the label */

   int (*handle)();	/* Handling procedure. */

   int *spec;           /* pointer to special stuff for object */

   int pushed;		/* wheter pushed */
   int focus; 		/* wheter focussed */
   int belowmouse;	/* Whether currently below the mouse */

   int active;		/* Whether active */
   int input;		/* Whether receiving input */
   int visible;         /* Whether being displayed */
   int radio;           /* Whether a radio object */
   int automatic;       /* Whether this object gets timer events */

   void (*call_back)();	/* The call-back routine */
   long argument;	/* Its argument */

   struct objs *next;	/* Next object in the form */
   struct objs *prev;	/* Previous object in the form */
   struct forms *form;	/* Form to which object belong */
} OBJECT;


/************   General Constants          ************/

#ifndef NULL
#define NULL                    0
#endif

#ifndef FALSE
#define FALSE                   0
#endif

#ifndef TRUE
#define TRUE                    1
#endif

/***** Placement *****/

#define PLACE_FREE		0
#define PLACE_SIZE		1
#define PLACE_ASPECT		2
#define PLACE_MOUSE		3

/***** Finds *****/

#define FIND_INPUT      0
#define FIND_AUTOMATIC  1
#define FIND_MOUSE      2

/***** Special Objects  *****/

#define BEGIN_GROUP		10000
#define END_GROUP		20000

/***** Alignments *****/

#define ALIGN_TOP		0
#define ALIGN_BOTTOM		1
#define ALIGN_LEFT		2
#define ALIGN_RIGHT		3
#define ALIGN_CENTER		4

/***** Bounding boxes *****/

#define NO_BOX			0
#define UP_BOX			1
#define DOWN_BOX		2
#define FLAT_BOX		3
#define BORDER_BOX		4
#define SHADOW_BOX		5

/***** Boundary Colors *****/

#define TOP_BOUND_COL		51
#define LEFT_BOUND_COL		55
#define BOT_BOUND_COL		40
#define RIGHT_BOUND_COL		35

#define COL1			47
#define MCOL			49
#define LCOL			0

#define BOUND_WIDTH		4.0

/***** Events *****/

#define DRAW			0
#define PUSH			1
#define RELEASE			2
#define ENTER			3
#define LEAVE			4
#define MOUSE			5
#define FOCUS			6
#define UNFOCUS			7
#define KEYBOARD		8
#define STEP			9

/***** Font *****/

#define FONT_NAME		"Helvetica"
#define FONT_BOLDNAME		"Helvetica-Bold"
#define FONT_ITALICNAME		"Helvetica-Oblique"

#define SMALL_FONT		8.0
#define NORMAL_FONT		11.0
#define LARGE_FONT		20.0

#define NORMAL_STYLE		0
#define BOLD_STYLE		1
#define ITALIC_STYLE		2


/************   General Routines           ************/

/***** In objects.c *****/

extern FORM *make_form();
extern OBJECT *make_object();

extern void add_object();
extern void delete_object();

extern void set_object_boxtype();
extern void set_object_color();
extern void set_object_label();
extern void set_object_lcol();
extern void set_object_lsize();
extern void set_object_align();

extern void show_object();
extern void hide_object();

extern void set_call_back();

extern void set_object_focus();

extern OBJECT *find_object();
extern OBJECT *find_first();
extern OBJECT *find_last();

extern void redraw_object();
extern void redraw_form();

extern int display_form();
extern void remove_form();

extern int handle_object();

/***** In forms.c *****/

extern FORM *current_form;

extern FORM *bgn_form();
extern void end_form();

extern OBJECT *bgn_group();
extern void end_group();

extern int show_form();
extern void hide_form();

extern OBJECT *do_forms();
extern OBJECT *check_forms();

extern void activate_form();
extern void deactivate_form();

extern OBJECT *EVENT;

/***** In events.c *****/

extern void fl_qdevice();
extern void fl_unqdevice();
extern int fl_isqueued();
extern long fl_qtest();
extern long fl_qread();
extern long fl_blkqread();
extern void fl_qreset();
extern void fl_qenter();
extern void fl_tie();

/***** In goodies.c *****/

extern void show_buttonbox();
extern void hide_buttonbox();
extern void set_buttonbox_label();

extern void show_message();
extern int show_question();
extern int show_choice();
extern char *show_input();

/***** In draw.c *****/

extern void get_pixel_size();
extern void get_mouse();

extern void set_clipping();
extern void unset_clipping();

extern void set_font();
extern void set_font_style();
extern float get_char_height();
extern float get_string_width();

extern void drw_box();
extern void drw_text_cursor();
extern void drw_text();
extern void drw_text_beside();

/***** In symbols.c *****/

extern int add_symbol();
extern int draw_symbol();

/************   Object Class: Box          ************/

/***** Class    *****/

#define BOX			1

/***** Types    *****/

/* See the bouding boxes */

/***** Defaults *****/

#define BOX_BOXTYPE		UP_BOX
#define BOX_COL1		COL1
#define BOX_LCOL		LCOL
#define BOX_ALIGN		ALIGN_CENTER

/***** Others   *****/

#define BOX_BW			BOUND_WIDTH

/***** Routines *****/

extern OBJECT *add_box();


/************   Object Class: Text         ************/

/***** Class    *****/

#define TEXT			2

/***** Types    *****/

#define NORMAL_TEXT		0
#define BOLD_TEXT		1
#define ITALIC_TEXT		2

/***** Defaults *****/

#define TEXT_BOXTYPE		NO_BOX
#define TEXT_COL1		COL1
#define TEXT_LCOL		LCOL
#define TEXT_ALIGN		ALIGN_LEFT

/***** Others   *****/

#define TEXT_BW			BOUND_WIDTH

/***** Routines *****/

extern OBJECT *add_text();


/************   Object Class: Button       ************/

/***** Class    *****/

#define BUTTON			11

/***** Types    *****/

#define NORMAL_BUTTON		0
#define PUSH_BUTTON		1
#define RADIO_BUTTON		2
#define HIDDEN_BUTTON		3
#define TOUCH_BUTTON		4
#define INOUT_BUTTON		5

/***** Defaults *****/

#define BUTTON_BOXTYPE		UP_BOX
#define BUTTON_COL1		COL1
#define BUTTON_COL2		COL1
#define BUTTON_LCOL		LCOL
#define BUTTON_ALIGN		ALIGN_CENTER

/***** Others   *****/

#define BUTTON_MCOL1		MCOL
#define BUTTON_MCOL2		MCOL
#define BUTTON_BW		BOUND_WIDTH

/***** Routines *****/

extern OBJECT *add_button();
extern void set_button();
extern int get_button();


/************   Object Class: Lightbutton  ************/

/***** Class    *****/

#define LIGHTBUTTON		12
#define TOPLIGHTBUTTON		14
#define RHTLIGHTBUTTON		15
#define BOTLIGHTBUTTON		16

/***** Types    *****/

    /* Same types as for buttons */

/***** Defaults *****/

#define LIGHTBUTTON_BOXTYPE	UP_BOX
#define LIGHTBUTTON_COL1	39
#define LIGHTBUTTON_COL2	3
#define LIGHTBUTTON_LCOL	LCOL
#define LIGHTBUTTON_ALIGN	ALIGN_CENTER

/***** Others   *****/

#define LIGHTBUTTON_TOPCOL	COL1
#define LIGHTBUTTON_MCOL	MCOL
#define LIGHTBUTTON_BW1		BOUND_WIDTH
#define LIGHTBUTTON_BW2		BOUND_WIDTH/2.0

/***** Routines *****/

extern OBJECT *add_lightbutton(), *add_rhtlightbutton(),
		*add_toplightbutton(), *add_botlightbutton();


/************   Object Class: Roundbutton  ************/

/***** Class    *****/

#define ROUNDBUTTON		13

/***** Types    *****/

    /* Same types as for buttons */

/***** Defaults *****/

#define ROUNDBUTTON_BOXTYPE	NO_BOX
#define ROUNDBUTTON_COL1	7
#define ROUNDBUTTON_COL2	3
#define ROUNDBUTTON_LCOL	LCOL
#define ROUNDBUTTON_ALIGN	ALIGN_CENTER

/***** Others   *****/

#define ROUNDBUTTON_TOPCOL	COL1
#define ROUNDBUTTON_MCOL	MCOL
#define ROUNDBUTTON_BW		BOUND_WIDTH

/***** Routines *****/

extern OBJECT *add_roundbutton();


/************   Object Class: Slider       ************/

/***** Class    *****/

#define SLIDER			21

/***** Types    *****/

#define VERT_SLIDER		0
#define HOR_SLIDER		1
#define VERT_FILL_SLIDER	2
#define HOR_FILL_SLIDER		3

/***** Defaults *****/

#define SLIDER_BOXTYPE		DOWN_BOX
#define SLIDER_COL1		COL1
#define SLIDER_COL2		COL1
#define SLIDER_LCOL		LCOL
#define SLIDER_ALIGN		ALIGN_BOTTOM

/***** Others   *****/

#define SLIDER_BW1		BOUND_WIDTH
#define SLIDER_BW2		BOUND_WIDTH*0.75

#define SLIDER_WIDTH		0.08

/***** Routines *****/

extern OBJECT *add_slider();
extern void set_slider_value();
extern float get_slider_value();
extern void set_slider_bounds();
extern void get_slider_bounds();
extern void set_slider();
extern float get_slider();
extern void set_slider_size();

/************   Object Class: Dial         ************/

/***** Class    *****/

#define DIAL			22

/***** Types    *****/

#define NORMAL_DIAL		0
#define LINE_DIAL		1

/***** Defaults *****/

#define DIAL_BOXTYPE		NO_BOX
#define DIAL_COL1		COL1
#define DIAL_COL2		37
#define DIAL_LCOL		LCOL
#define DIAL_ALIGN		ALIGN_BOTTOM

/***** Others   *****/

#define DIAL_TOPCOL		COL1
#define DIAL_BW			BOUND_WIDTH

/***** Routines *****/

extern OBJECT *add_dial();
extern void set_dial_value();
extern float get_dial_value();
extern void set_dial_bounds();
extern void get_dial_bounds();
extern void set_dial();
extern float get_dial();


/************   Object Class: Positioner   ************/

/***** Class    *****/

#define POSITIONER		23

/***** Types    *****/

#define NORMAL_POSITIONER	0

/***** Defaults *****/

#define POSITIONER_BOXTYPE	DOWN_BOX
#define POSITIONER_COL1		COL1
#define POSITIONER_COL2		1
#define POSITIONER_LCOL		LCOL
#define POSITIONER_ALIGN	ALIGN_BOTTOM

/***** Others   *****/

#define POSITIONER_BW		BOUND_WIDTH

/***** Routines *****/

extern OBJECT *add_positioner();
extern void set_positioner_xvalue();
extern float get_positioner_xvalue();
extern void set_positioner_xbounds();
extern void get_positioner_xbounds();
extern void set_positioner_yvalue();
extern float get_positioner_yvalue();
extern void set_positioner_ybounds();
extern void get_positioner_ybounds();
extern void set_x_positioner();
extern void set_y_positioner();
extern void get_positioner();


/************   Object Class: Input        ************/

/***** Class    *****/

#define INPUT			31

/***** Types    *****/

#define NORMAL_INPUT		0

/***** Defaults *****/

#define INPUT_BOXTYPE		DOWN_BOX
#define INPUT_COL1		13
#define INPUT_COL2		5
#define INPUT_LCOL		LCOL
#define INPUT_ALIGN		ALIGN_LEFT

/***** Others   *****/

#define INPUT_TCOL		LCOL
#define INPUT_CCOL		4
#define INPUT_BW		BOUND_WIDTH
#define INPUT_MAX		128

/***** Routines *****/

extern OBJECT *add_input();
extern void set_input();
extern void set_input_color();
extern char *get_input();


/************   Object Class: Menu         ************/

/***** Class    *****/

#define MENU			41

/***** Types    *****/

#define TOUCH_MENU		0
#define PUSH_MENU		1

/***** Defaults *****/

#define MENU_BOXTYPE		BORDER_BOX
#define MENU_COL1		55
#define MENU_COL2		37
#define MENU_LCOL		LCOL
#define MENU_ALIGN		ALIGN_CENTER

/***** Others   *****/

#define MENU_BW			BOUND_WIDTH
#define MENU_MAX		300

/***** Routines *****/

extern OBJECT *add_menu();
extern void set_menu();
extern void addto_menu();
extern long get_menu();


/************   Object Class: Default      ************/

/***** Class    *****/

#define DEFAULT			51

/***** Types    *****/

#define RETURN_DEFAULT		0
#define ALWAYS_DEFAULT		1

/***** Defaults *****/

/***** Others   *****/

/***** Routines *****/

extern OBJECT *add_default();
extern char get_default();


/************   Object Class: Clock        ************/

/***** Class    *****/

#define CLOCK			61

/***** Types    *****/

#define SQUARE_CLOCK		0
#define ROUND_CLOCK		1

/***** Defaults *****/

#define CLOCK_BOXTYPE		UP_BOX
#define CLOCK_COL1		37
#define CLOCK_COL2		42
#define CLOCK_LCOL		LCOL
#define CLOCK_ALIGN		ALIGN_BOTTOM

/***** Others   *****/

#define CLOCK_TOPCOL		COL1
#define CLOCK_BW		BOUND_WIDTH

/***** Routines *****/

extern OBJECT *add_clock();
extern void get_clock();


/************   Object Class: Browser      ************/

/***** Class    *****/

#define BROWSER			71

/***** Types    *****/

#define NORMAL_BROWSER		0

/***** Defaults *****/

#define BROWSER_BOXTYPE		DOWN_BOX
#define BROWSER_COL1		COL1
#define BROWSER_COL2		LCOL
#define BROWSER_LCOL		LCOL
#define BROWSER_ALIGN		ALIGN_BOTTOM

/***** Others   *****/

#define BROWSER_BW		BOUND_WIDTH
#define BROWSER_MAXSTR		20000

/***** Routines *****/

extern OBJECT *add_browser();
extern int set_browser();
extern void addto_browser();
extern void set_browser_font();


/************   Object Class: Free         ************/

/***** Class    *****/

#define FREE			101

/***** Types    *****/

#define NORMAL_FREE		1
#define SLEEPING_FREE		2
#define INPUT_FREE		3
#define CONTINUOUS_FREE		4
#define ALL_FREE		5

/***** Defaults *****/

/***** Others   *****/

/***** Routines *****/

extern OBJECT *add_free();
extern void set_free();


/*
 * Local Variables:
 * mode: C
 * tab-width: 8
 * c-basic-offset: 4
 * indent-tabs-mode: t
 * End:
 * ex: shiftwidth=4 tabstop=8
 */
