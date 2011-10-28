
# Usage message. This is printed to standard error if the script
# is invoked incorrectly.
#
set ::usage [string trim [subst {
Usage: $argv0 OPTIONS

    Options are:
        -dir <test-dir> 
        -cache <cache-file> 
        -pattern <glob-pattern>
}]]

# Load required packages. As well as Tk and Tkhtml this script needs
# Img (to do image compression, otherwise the images are all very large
# blobs) and sqlite3 (to store saved images in a database file).
#
if {[info exists auto_path]} {
  set auto_path [concat . $auto_path]
}
package require Tk
package require Tkhtml 3.0
package require Img
package require sqlite3

# Load hv3 to do the rendering
source [file join [file dirname [info script]] hv3.tcl]

#--------------------------------------------------------------------------
# A little database API for use within this app. This API serves to 
# encapsulate the use of SQLite:
#
#     dbInit
#     dbRetrieve
#     dbStore
#     dbClose
#
proc dbInit {filename} {
  for {set n 0} {[llength [info commands hv3_db_$n]]>0} {incr n} {}
  set db "hv3_db_$n"
  sqlite3 $db $filename
  catch {
    $db eval {CREATE TABLE hv3_images(url TEXT PRIMARY KEY, image BLOB)}
  }
  return $db
}
proc dbRetrieve {db url} {
  $db onecolumn {SELECT image FROM hv3_images WHERE url = $url}
}
proc dbStore {db url data} {
  $db eval {REPLACE INTO hv3_images(url, image) VALUES($url, $data)}
}
proc dbClose {db} {
  $db close
}
#--------------------------------------------------------------------------

# This proc uses the hv3 object $hv3 to render the document at URL
# $url. It then creates an image from the rendered document and returns
# the binary image data.
#
proc snapshot {hv3 url} {
  # Use a temp file to write the image data to instead of returning
  # it directly to Tcl. For whatever reason, this seems to be about 
  # thirty percent faster.
  set tmp_file_name "/tmp/tst_hv3[pid].jpeg"

  # Load the document at $url into .hv3
  hv3Goto $hv3 $url

  # This block sets variable $data to contain the binary image data
  # for the rendered document - via a Tk image and temp file entry.
  set img [${hv3}.html image]
  $img write $tmp_file_name -format jpeg
  image delete $img
  set fd [open $tmp_file_name]
  fconfigure $fd -encoding binary -translation binary
  set data [read $fd]
  close $fd
  file delete -force $tmp_file_name

  # Return the image data.
  return $data
}

# Create an hv3 object. Configure the width of the html window to 800 
# pixels. Because the .hv3 object will never actually be packed into 
# the gui, the configured width is used as the viewport width by the
# [.hv3.html image] command. (If .hv3 were packed, then the actual
# viewport width would be used instead.)
#
hv3Init .hv3
.hv3.html configure -width 800

# Create the application gui:
#
#   .main                -> frame
#   .main.canvas         -> canvas widget
#   .main.vsb            -> vertical scrollbar for canvas widget
#
#   .control             -> frame
#   .control.displayold  -> button
#   .control.displaynew  -> button
#   .control.copynewtodb -> button
#   .control.next        -> button
#
frame     .main
canvas    .main.canvas
scrollbar .main.vsb

.main.canvas configure -width 800 -height 600 -yscrollcommand ".main.vsb set"
.main.vsb    configure -command ".main.canvas yview" -orient vertical
pack .main.canvas -fill both -expand true -side left
pack .main.vsb    -fill y    -expand true
pack .main        -fill both -expand true

frame     .control
foreach {b t} [list \
  displayold  "Display Old"          \
  displaynew  "Display New"          \
  copynewtodb "Copy New Image to Db" \
  skip        "Skip to next test"    \
] {
  button .control.$b
  .control.$b configure -command [list click $b]
  .control.$b configure -text $t
  pack .control.$b -side left -expand true -fill x
}
pack .control -fill x -expand true

# This proc is called when one of the buttons is clicked. The argument
# is the unqualified name of the widget (i.e. "displayold").
#
proc click {b} {
  switch -- $b {
    displayold  {
      .main.canvas itemconfigure oldimage -state normal
      .main.canvas itemconfigure newimage -state hidden
      .control.displayold configure -state disabled
      .control.displaynew configure -state normal
    }
    displaynew  {
      .main.canvas itemconfigure oldimage -state hidden
      .main.canvas itemconfigure newimage -state normal
      .control.displaynew configure -state disabled
      if {[llength [info commands oldimage]]>0} {
        .control.displayold configure -state normal
      } else {
        .control.displayold configure -state disabled
      }
    }
    copynewtodb {
      set ::continue 2
    }
    skip        {
      set ::continue 1
    }
  }
}

proc do_image_test {db url} {
  # Create a snapshot of the URL.
  set newdata [snapshot .hv3 $url]

  # Retrieve the cached version, if any
  set olddata [dbRetrieve $db $url]

  # If the cached data matches the new snapshot, there is no need for
  # user interaction. Just print a message to stdout to say we're happy 
  # with this rendering.
  if {$newdata==$olddata} {
    puts "SUCCESS $url"
    return
  }

  .control.skip        configure -state normal
  .control.copynewtodb configure -state normal
  .control.displayold  configure -state normal
  .control.displaynew  configure -state normal

  image create photo newimage -data $newdata
  .main.canvas create image 0 0 -image newimage -anchor nw -tag newimage
  if {$olddata!={}} {
    image create photo oldimage -data $olddata
    .main.canvas create image 0 0 -image oldimage -anchor nw -tag oldimage
  }
  set dim [.main.canvas bbox all]
  .main.canvas configure -scrollregion $dim

  .control.displaynew invoke
  set ::continue 0
  vwait ::continue
  if {$::continue==2} {
    puts "SUCCESS $url"
    dbStore $db $url $newdata
  } else {
    puts "FAILURE $url"
  }
  image delete newimage
  catch { image delete oldimage }
}

swproc main_args {{cache {}} {dir {}} {pattern *.htm}} {
  if {$cache=={}} {
    error "Error: Required -cache option not specified"
  }
  if {$dir=={}} {
    error "Error: Required -dir option not specified"
  }
  set dir [file normalize $dir]
  uplevel [list set cache $cache]
  uplevel [list set dir $dir]
  uplevel [list set pattern $pattern]
}
proc main {argv} {
  if {[catch {eval main_args $argv} msg]} {
    puts stderr $msg
    puts stderr $::usage
    exit -1
  }
  set db [dbInit $cache]
  set url_list [lsort [glob [file join $dir $pattern]]]
  foreach url $url_list {
    do_image_test $db $url
  }
}
main $argv

