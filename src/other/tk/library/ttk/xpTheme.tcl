#
# $Id$
#
# Ttk widget set: XP Native theme
#
# @@@ todo: spacing and padding needs tweaking

namespace eval ttk {

    style theme settings xpnative {

	style configure . \
	    -background SystemButtonFace \
	    -foreground SystemWindowText \
	    -selectforeground SystemHighlightText \
	    -selectbackground SystemHighlight \
	    -font TkDefaultFont \
	    ;

	style map "." \
	    -foreground [list disabled SystemGrayText] \
	    ;

	style configure TButton -padding {1 1} -width -11
	style configure TRadiobutton -padding 2
	style configure TCheckbutton -padding 2
	style configure TMenubutton -padding {8 4} -anchor w

	style configure TNotebook -tabmargins {2 2 2 0}
	style map TNotebook.Tab \
	    -expand [list selected {2 2 2 2}]

	# Treeview:
	style configure Heading -font TkHeadingFont
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

	style configure TLabelframe -foreground "#0046d5"

	# OR: -padding {3 3 3 6}, which some apps seem to use.
	style configure TEntry -padding {2 2 2 4}
	style map TEntry \
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

	style configure Toolbutton -padding {4 4}
    }
}
