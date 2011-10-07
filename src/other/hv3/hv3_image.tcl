
###########################################################################
# hv3_image.tcl --
#
#     This file contains code to add image capabilities to the widget.
#     The public interface to this file are the commands:
#
#         image_init HTML
#


#--------------------------------------------------------------------------
# Global variables section
set ::hv3_image_name 0

#--------------------------------------------------------------------------

# imageCallback --
# 
#         imageCmd IMAGE-NAME DATA
#
#     This proc is called when an image requested by the -imagecmd callback
#     ([imageCmd]) has finished downloading. The first argument is the name of
#     a Tk image. The second argument is the downloaded data (presumably a
#     binary image format like gif). This proc sets the named Tk image to
#     contain the downloaded data.
#
proc imageCallback {name data} {
    if {[info commands $name] == ""} return 
    $name put $data
}

# imageCmd --
# 
#         imageCmd HTML URL
#
#     This proc is registered as the -imagecmd script for the Html widget.
#
proc imageCmd {HTML url} {
    set name hv3_image[incr ::hv3_image_name]
    image create photo $name

    set fullurl [url_resolve [$HTML var url] $url]
    url_fetch $fullurl -script [list imageCallback $name] -binary -type Image

    return [list $name [list image delete $name]]
}


set ::html_image_format jpeg

proc image_to_serial {img} {
  return [$img data -format $::html_image_format]
}

proc image_savefile {HTML} {
  set t [time {set img [$HTML image]}]
  # puts "IMAGE-TIME: $t"
  $img write tkhtml.$::html_image_format -format $::html_image_format
  image delete $img
}

proc image_savetest {HTML} {
  set url [$HTML var url]
  puts "SAVING $url..."

  catch {
    [.html var cache] eval {CREATE TABLE tests(url PRIMARY KEY, data BLOB);}
  }
  set t [time {set img [$HTML image]}]
  # puts "IMAGE-TIME: $t"

  set data [image_to_serial $img]
  [$HTML var cache] eval {REPLACE INTO tests VALUES($url, $data);}
  image delete $img
  puts "DONE"
}

proc image_runtests {HTML} {
  image_800x600
  set db [.html var cache]
  set runtime [time {
    $db eval {SELECT oid as id, url, data FROM tests order by url} v {
      gui_goto $v(url)
  
      #after 100 {set ::hv3_runtests_var 0}
      #vwait ::hv3_runtests_var
  
      set t [time {set img [$HTML image]}]
      # puts "IMAGE-TIME: ($url) $t"
      set newdata [image_to_serial $img]
  
      if {$v(data) != $newdata} {
        puts "TEST FAILURE: ($v(url)) -> $v(id).$::html_image_format"
        $img write $v(id)_2.$::html_image_format -format $::html_image_format
      } else {
          puts "TEST SUCCESSFUL: ($v(url)) $v(id)"
      }
      image delete $img
    }
  }]

  puts "RUNTESTS FINISHED - $runtime"
}

proc image_800x600 {} {
    wm geometry . 800x600
    update
}

proc image_init {HTML} {
    .m add cascade -label {Image Tests} -menu [menu .m.image]
    if {0 == [catch {uplevel #0 {package require Img}}]} {
    .m.image add command -label {800x600} -command "image_800x600"
    .m.image add separator
    .m.image add command -label {Save file...} -command "image_savefile $HTML"
    .m.image add command -label {Save test case} -command "image_savetest $HTML"
    .m.image add separator
    .m.image add command -label {Run all tests} -command "image_runtests $HTML"
    } else {
        .m add command -label "Image tests require Tcl package Img"
    }

    $HTML configure -imagecmd [list imageCmd $HTML]
}
