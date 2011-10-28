namespace eval hv3 { set {version($Id$)} 1 }

#
# This file contains Tcl code for loading file:// URIs in the hv3 web browser
# application. Directories, text and binary files are all handled by code
# within.
#
# Procs:
#
#     ::hv3::request_file
#     ::hv3::directoryIndex
#     ::hv3::directoryIndex_CreateIcons
#
# The only interface accessed by code external to this file is 
# [::hv3::request_file].
#

################################################################################
#
#  Bugs and TODOs
#
#   * Have a smart way to use internal icons, instead of the "replace:" trick
#       since it deletes the image on page "unloading", forcing to recreate
#       icons on each  directoryIndex call
#   * Handle mime/types to show relevant file icons.
#   * ...
#
################################################################################

namespace eval hv3 {

##############################################################################
#
# Procedure: directoryIndex
#
#   Lists given directory content, generates an HTML representation of
#   manifest/index and return this as a string.
#
# Arguments:
#   directory - absolute path to directory being listed
#
# Result:
#   data - string containing HTML index page content
#
# Side effects:
#   None.
#
##############################################################################
proc directoryIndex {directory} {

    array set iconsArray [directoryIndex_CreateIcons]
    set data {
        <html>
          <head>
          </head>
          <body>
    }
    if {[string length $directory]>40} {
        append data "<H1 class=heading>Index of<br>\
            <font size=2>$directory</font></H1>"
    } elseif {[string length $directory]>20} {
        append data "<H1 class=heading>Index of<br>$directory</H1>"
    } else {
        append data "<H1 class=heading>Index of $directory</H1>"
    }
    
    # special treatment for parent directory
    append data "\n<img src=replace:$iconsArray(bitmap_folder_icon)>\
        &nbsp;<a href='[file dirname $directory]'>.. (parent directory)</a><br>"
    
    # List, sort and create an icon+link entry for each file/folder
    set ex 0
    foreach entry [lsort [glob -nocomplain -directory $directory *]] {
        append data "<div class=direntry>"
        if {[file isdirectory $entry]} {
            append data "\n<img src=replace:$iconsArray(bitmap_folder_icon)>"
        } else {
            append data "\n<img src=replace:$iconsArray(bitmap_file_icon)>"
        }
        set fname [file tail $entry]

        append data "&nbsp;<a href='$entry'>$fname</a><br>"
        append data "</div> "

        set len [string length $fname]
        incr len 10
        if {$len > $ex} {set ex $len}
    }

    append data [subst {
      <style>
      .direntry {
        display: inline-block;
        width: ${ex}ex;
      }
      </style>
    }]

    append data {
          </body>
        </html>
    }
    
    return $data
    
};#End of procedure directoryIndex


################################################################################
#
# Procedure: directoryIndex_CreateIcons
#
#   Creates bitmap icons for directory indexing
#
# Arguments:
#   None.
#
# Result:
#   namesPairList - pair list with image names
#
# Side effects:
#   Creates bitmap images
#
# Keywords:
#   
#
################################################################################
proc directoryIndex_CreateIcons {} {
    set namesArray(bitmap_folder_icon) \
        [image create bitmap -foreground black -background yellow\
            -data {
                #define d_width 16
                #define d_height 16
                static unsigned char d_bits[] = {
                    0x00, 0x00, 0x3c, 0x00, 0x42, 0x00, 0x81, 0x3f, 0x7f, 0x40, 
                    0x01, 0xc0, 0x01, 0xc0, 0x01, 0xc0, 0x01, 0xc0, 0x01, 0xc0, 
                    0x01, 0xc0, 0x01, 0xc0, 0x01, 0xc0, 0xff, 0xff, 0xfe, 0xff, 
                    0x00, 0x00};
            }\
            -maskdata {
                #define m_width 16
                #define m_height 16
                static unsigned char m_bits[] = {
                    0x00, 0x00, 0x3c, 0x00, 0x7e, 0x00, 0xff, 0x3f, 0xff, 0x7f,
                    0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff,
                    0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xfe, 0xff, 
                    0x00, 0x00};
            }]

    set namesArray(bitmap_file_icon) \
        [image create bitmap -foreground black -background white\
            -data {
                #define d4_width 16
                #define d4_height 16
                static unsigned char d4_bits[] = {
                    0x00, 0x00, 0xfe, 0x7f, 0x62, 0x40, 0x72, 0x55, 0x7a, 0x4a, 
                    0x7e, 0x40, 0x7e, 0x5f, 0x02, 0x40, 0xfa, 0x5f, 0x02, 0x40, 
                    0xfa, 0x5f, 0x02, 0x40, 0xfa, 0x5f, 0x02, 0x40, 0xfe, 0x7f, 
                    0x00, 0x00};
            }\
            -maskdata {
                #define m_width 16
                #define m_height 16
                static unsigned char m_bits[] = {
                    0x00, 0x00, 0xfe, 0x7f, 0xfe, 0x7f, 0xfe, 0x7f, 0xfe, 0x7f, 
                    0xfe, 0x7f, 0xfe, 0x7f, 0xfe, 0x7f, 0xfe, 0x7f, 0xfe, 0x7f, 
                    0xfe, 0x7f, 0xfe, 0x7f, 0xfe, 0x7f, 0xfe, 0x7f, 0xfe, 0x7f, 
                    0x00, 0x00};
            }]
    set namesPairList [array get namesArray]
    return $namesPairList
};#End of procedure directoryIndex_CreateIcons


################################################################################

# Handle a file:// URI.
#
proc request_file {downloadHandle} {

    # Extract the "path" and "authority" components from the URI
    set uri_obj [::tkhtml::uri [$downloadHandle cget -uri]]
    set path      [$uri_obj path]
    set authority [$uri_obj authority]
    $uri_obj destroy

    set filename $path
    if {$::tcl_platform(platform) eq "windows" && $authority ne ""} {
        set filename "$authority:$path"
    }
    
    set data ""
    if {[file readable $filename]} {
    
        if {[file isdirectory $filename]} {

            set data [directoryIndex $filename]

        } else {
            # Read the file from the file system. The [open] or [read] command
            # might throw an exception. No problem, the hv3 widget will catch
            # it and automatically deem the request to have failed.
            #
            # Unless the expected mime-type begins with the string "text", 
            # configure the file-handle for binary mode.

	    # If $downloadHandle has -encoding, read data is converted by
	    # $downloadHandle itself.

            set fd [open $filename]

	    # Always uses binary encoding.
	    # $download is responsible to convert it.
	    #
	    fconfigure $fd -encoding binary

            if {![string match text* [$downloadHandle cget -mimetype]]} {
              fconfigure $fd -translation binary
	    }
            set data [read $fd]
            close $fd
        }
        
    } else {
    
        # Return error document
        if {[file exists $filename]} {
            set data "<H3>Reading denied</H3> You're not allowed to read $filename"
            set errmsg "Unreadable path: $filename"
        } else {
            set data "<H3>Not found</H3> Missing $filename"
            set errmsg "Not found: $filename"
        }
        #$downloadHandle fail "Unreadable path: $filename"
    }

    $downloadHandle finish $data
}

};# End of [namespace eval hv3]
################################################################################
