#
# $Id$
#
# Ttk widget set: Windows Native theme
#

namespace eval ttk {

    style theme settings winnative {

	style configure "." \
	    -background SystemButtonFace \
	    -foreground SystemWindowText \
	    -selectforeground SystemHighlightText \
	    -selectbackground SystemHighlight \
	    -troughcolor SystemScrollbar \
	    -font TkDefaultFont \
	    ;

	style map "." -foreground [list disabled SystemGrayText] ;
        style map "." -embossed [list disabled 1] ;

	style configure TButton -width -11 -relief raised -shiftrelief 1
	style configure TCheckbutton -padding "2 4"
	style configure TRadiobutton -padding "2 4"
	style configure TMenubutton \
	    -padding "8 4" -arrowsize 3 -relief raised -anchor w

	style map TButton -relief {{!disabled pressed} sunken}

	style configure TEntry \
	    -padding 2 -selectborderwidth 0 -insertwidth 1
	style map TEntry \
	    -fieldbackground \
	    	[list readonly SystemButtonFace disabled SystemButtonFace] \
	    -selectbackground [list !focus SystemWindow] \
	    -selectforeground [list !focus SystemWindowText] \
	    ;

	style configure TCombobox -padding 2
	style map TCombobox \
	    -selectbackground [list !focus SystemWindow] \
	    -selectforeground [list !focus SystemWindowText] \
	    -foreground	[list {readonly focus} SystemHighlightText] \
	    -focusfill	[list {readonly focus} SystemHighlight] \
	    ;

	style configure TLabelframe -borderwidth 2 -relief groove

	style configure Toolbutton -relief flat -padding {8 4}
	style map Toolbutton -relief \
	    {disabled flat selected sunken  pressed sunken  active raised}

	style configure TScale -groovewidth 4

	style configure TNotebook -tabmargins {2 2 2 0}
	style configure TNotebook.Tab -padding {3 1} -borderwidth 1
	style map TNotebook.Tab -expand [list selected {2 2 2 0}]

	# Treeview:
	style configure Heading -font TkHeadingFont -relief raised
	style configure Row -background SystemWindow
	style configure Cell -background SystemWindow
	style map Row \
	    -background [list selected SystemHighlight] \
	    -foreground [list selected SystemHighlightText] ;
	style map Cell \
	    -background [list selected SystemHighlight] \
	    -foreground [list selected SystemHighlightText] ;
	style map Item \
	    -background [list selected SystemHighlight] \
	    -foreground [list selected SystemHighlightText] ;

        style configure TProgressbar -borderwidth 0 -background SystemHighlight
    }
}
