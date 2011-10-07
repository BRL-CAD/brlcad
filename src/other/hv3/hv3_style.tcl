
###########################################################################
# hv3_style.tcl --
#
#     This file contains code to implement stylesheet functionality.
#     The public interface to this file are the commands:
#
#         style_init HTML
#         style_newdocument HTML
#

#--------------------------------------------------------------------------
# Global variables section
set ::hv3_style_count 0

#--------------------------------------------------------------------------

# style_init --
#
#         style_init HTML
#
#     This is called just after the html widget ($HTML) is created. The two
#     handler commands are registered.
#
proc style_init {HTML} {
    $HTML handler node link    "styleHandleLink $HTML"
    $HTML handler script style "styleHandleStyle $HTML"
}

# style_newdocument --
#
#         style_newdocument HTML
#
#     This should be called before each new document begins loading (i.e. from
#     [gui_goto]).
#
proc style_newdocument {HTML} {
    set ::hv3_style_count 0
}

# styleHandleStyle --
#
#     styleHandleStyle HTML SCRIPT
#
proc styleHandleStyle {HTML script} {
  set id author.[format %.4d [incr ::hv3_style_count]]
  styleCallback $HTML [$HTML var url] $id $script
}

# styleUrl --
#
#     styleUrl BASE-URL URL
#
proc styleUrl {baseurl url} {
    return [url_resolve $baseurl $url]
}

# styleCallback --
#
#     styleCallback HTML URL ID STYLE-TEXT
#
proc styleCallback {HTML url id style} {
    $HTML style \
        -id $id \
        -importcmd [list styleImport $HTML $id] \
        -urlcmd [list styleUrl $url] \
        $style
}

# styleImport --
#
#     styleImport HTML PARENTID URL
#
proc styleImport {HTML parentid url} {
    set id ${parentid}.[format %.4d [incr ::hv3_style_count]]
    url_fetch $url -script [list styleCallback $HTML $url $id] -type Stylesheet
}

# styleHandleLink --
#
#     styleHandleLink HTML NODE
#
proc styleHandleLink {HTML node} {
    if {[$node attr rel] == "stylesheet"} {
        # Check if the media is Ok. If so, download and apply the style.
        set media [$node attr -default "" media]
        if {$media == "" || [regexp all $media] || [regexp screen $media]} {
            set id author.[format %.4d [incr ::hv3_style_count]]
            set url [url_resolve [$HTML var url] [$node attr href]]
            set cmd [list styleCallback $HTML $url $id] 
            url_fetch $url -script $cmd -type Stylesheet
        }
    }
}

