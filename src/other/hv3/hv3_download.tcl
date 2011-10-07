namespace eval hv3 { set {version($Id$)} 1 }

# ::hv3::filedownload
#
# Each currently downloading file is managed by an instance of the
# following object type. All instances in the application are managed
# by the [::hv3::the_download_manager] object, an instance of
# class ::hv3::downloadmanager (see below).
#
# SYNOPSIS:
#
#     set obj [::hv3::filedownload %AUTO% ?OPTIONS?]
#
#     $obj set_destination $PATH
#     $obj append $DATA
#     $obj finish
#
# Options are:
#
#     Option        Default   Summary
#     -------------------------------------
#     -source       ""        Source of download (for display only)
#     -cancelcmd    ""        Script to invoke to cancel the download
#
namespace eval ::hv3::filedownload {

  proc new {me args} {
    upvar #0 $me O

    # Variables used by fdgui:
    set O(fdgui.status)               ""
    set O(fdgui.progress)             ""
    set O(fdgui.buttontext)           Cancel

    # The destination path (in the local filesystem) and the corresponding
    # tcl channel (if it is open). These two variables also define the 
    # three states that this object can be in:
    #
    # INITIAL:
    #     No destination path has been provided yet. Both O(fdgui.destination)
    #     and O(myChannel) are set to an empty string.
    #
    # STREAMING:
    #     A destination path has been provided and the destination file is
    #     open. But the download is still in progress. Neither
    #     O(fdgui.destination) nor O(myChannel) are set to an empty string.
    #
    # FINISHED:
    #     A destination path is provided and the entire download has been
    #     saved into the file. We're just waiting for the user to dismiss
    #     the GUI. In this state, O(myChannel) is set to an empty string but
    #     O(fdgui.destination) is not.
    #
    set O(fdgui.destination) ""
    set O(myChannel) ""
  
    # Buffer for data while waiting for a file-name. This is used only in the
    # state named INITIAL in the above description. The $O(myIsFinished) flag
    # is set to true if the download is finished (i.e. [finish] has been 
    # called).
    set O(myBuffer) ""
    set O(myIsFinished) 0
  
    set O(-source) ""
    set O(-cancelcmd) ""
    
    # Total bytes downloaded so far.
    set O(myDownloaded) 0
  
    # Total bytes expected (i.e. size of the download)
    set O(myExpected) 0

    set O(speed.afterid) ""
    set O(speed.downloaded) 0
    set O(speed.milliseconds) 0
    set O(speed.speed) ""

    eval $me configure $args

    $me CalculateSpeed
  }

  proc destroy {me} {
    upvar #0 $me O
    catch { close $O(myChannel) }
    catch { after cancel $O(speed.afterid) }
    array unset $me
    rename $me {}
  }

  proc CalculateSpeed {me} {
    upvar #0 $me O

    set time [clock milliseconds]

    set bytes  [expr {$O(myDownloaded)-$O(speed.downloaded)}]
    set clicks [expr {$time-$O(speed.milliseconds)}]

    set O(speed.speed) [
      format "%.1f KB/sec" [expr {double($bytes)/double($clicks)}]
    ]
    set O(speed.downloaded) $O(myDownloaded)
    set O(speed.milliseconds) $time

    set O(speed.afterid) [after 2000 [list $me CalculateSpeed]]
    $me Updategui
  }

  proc set_destination {me dest isFinished} {
    upvar #0 $me O
    if {$isFinished} {
      set O(myIsFinished) 1
    }

    # It is an error if this method has been called before.
    if {$O(fdgui.destination) ne ""} {
      error "This ::hv3::filedownloader already has a destination!"
    }

    if {$dest eq ""} {
      # Passing an empty string to this method cancels the download.
      # This is for conveniance, because [tk_getSaveFile] returns an 
      # empty string when the user selects "Cancel".
      $me Cancel
      destroy $me
    } else {
      # Set the O(fdgui.destination) variable and open the channel to the
      # file to write. Todo: An error could occur opening the file.
      set O(fdgui.destination) $dest
      set O(myChannel) [open $O(fdgui.destination) w]
      fconfigure $O(myChannel) -encoding binary -translation binary

      # If a buffer has accumulated, write it to the new channel.
      puts -nonewline $O(myChannel) $O(myBuffer)
      set O(myBuffer) ""

      # If the O(myIsFinished) flag is set, then the entire download
      # was already in the buffer. We're finished.
      if {$O(myIsFinished)} {
        $me finish {} {}
      }

      ::hv3::the_download_manager manage $me
    }
  }

  # This internal method is called to cancel the download. When this
  # returns the object will have been destroyed.
  #
  proc Cancel {me } {
    upvar #0 $me O
    # Evaluate the -cancelcmd script and destroy the object.
    eval $O(-cancelcmd)
    if {$O(fdgui.destination) ne ""} {
      catch {close $O(myChannel)}
      catch {file delete $O(fdgui.destination)}
    }
  }

  # Update the GUI to match the internal state of this object.
  #
  proc Updategui {me } {
    upvar #0 $me O

    set bytes $O(myDownloaded)
    if {$O(myIsFinished)} {
      set O(fdgui.progress) 100
      set O(fdgui.status) "Finished ($bytes bytes)" 
      set O(fdgui.buttontext) "Dismiss"
    } else {
      set speed $O(speed.speed)
      set total $O(myExpected)
      if {$total eq "" || $total == 0} {set total 1}
      set progress "[expr {$bytes/1024}] of [expr {$total/1024}] KB"

      set O(fdgui.progress) [expr {$bytes*100/$total}]
      set O(fdgui.status)   "$progress ($O(fdgui.progress)%) at $speed"
    }
  }

  proc append {me handle data} {
    upvar #0 $me O
    set O(myExpected) [$handle cget -expectedsize]
    if {$O(myChannel) ne ""} {
      puts -nonewline $O(myChannel) $data
      set O(myDownloaded) [file size $O(fdgui.destination)]
    } else {
      ::append O(myBuffer) $data
      set O(myDownloaded) [string length $O(myBuffer)]
    }
    $me Updategui
  }

  # Called by the driver download-handle when the download is 
  # complete. All the data will have been already passed to [append].
  #
  proc finish {me handle data} {
    upvar #0 $me O
    if {$data ne ""} {
      $me append $handle $data
    }

    # If the channel is open, close it.
    if {$O(myChannel) ne ""} {
      close $O(myChannel)
      set O(myChannel) ""
    }

    # If O(myIsFinished) flag is not set, set it and then set O(myElapsed) to
    # indicate the time taken by the download.
    if {!$O(myIsFinished)} {
      set O(myIsFinished) 1
    }

    # Update the GUI.
    $me Updategui

    # Delete the download request.
    if {$handle ne ""} {
      $handle destroy
    }
  }

  # Query interface used by ::hv3::downloadmanager GUI. It cares about
  # four things: 
  #
  #     * the percentage of the download has been completed, and
  #     * the state of the download (either "Downloading" or "Finished").
  #     * the source URI
  #     * the destination file
  #
  proc state {me } {
    upvar #0 $me O
    if {$O(myIsFinished)} {return "Finished"}
    return "Downloading"
  }
  proc percentage {me } {
    upvar #0 $me O
    if {$O(myIsFinished)} {return 100}
    if {$O(myExpected) eq "" || $O(myExpected) == 0} {
      return [expr {$O(myDownloaded) > 0 ? 50 : 0}]
    }
    return [expr double($O(myDownloaded)) / double($O(myExpected)) * 100]
  }
  proc bytes {me} {
    upvar #0 $me O
    return $O(myDownloaded)
  }
  proc expected {me} {
    upvar #0 $me O
    return $O(myExpected)
  }
  proc destination {me} {
    upvar #0 $me O
    return $O(fdgui.destination)
  }
  proc speed {me} {
    upvar #0 $me O
    return $O(speed.speed)
  }
}
::hv3::make_constructor ::hv3::filedownload

namespace eval ::hv3::fdgui {

  set DelegateOption(-command) myButton

  proc new {me fd args} {
    upvar #0 $me O

    set f [frame $O(win).frame]
    set spacer [frame $O(win).spacer -background #c3c3c3 -width 15] 

    ::hv3::label $f.label_source   -text Source:      -anchor w
    ::hv3::label $f.label_dest     -text Destination: -anchor w
    ::hv3::label $f.label_status   -text Status:      -anchor w
    ::hv3::label $f.label_progress -text Progress:    -anchor w

    ::hv3::label     $f.source   -textvar ${fd}(-source)           -anchor w
    ::hv3::label     $f.dest     -textvar ${fd}(fdgui.destination) -anchor w
    ::hv3::label     $f.status   -textvar ${fd}(fdgui.status)      -anchor w
    ttk::progressbar $f.progress -variable ${fd}(fdgui.progress) 

    grid $f.label_source   -column 0 -row 0 -sticky ew
    grid $f.label_dest     -column 0 -row 1 -sticky ew
    grid $f.label_status   -column 0 -row 2 -sticky ew
    grid $f.label_progress -column 0 -row 3 -sticky ew

    grid $f.source   -column 1 -row 0 -sticky ew -padx 10 
    grid $f.dest     -column 1 -row 1 -sticky ew -padx 10 
    grid $f.status   -column 1 -row 2 -sticky ew -padx 10 
    grid $f.progress -column 1 -row 3 -sticky ew -padx 10 

    grid columnconfigure $f 1 -weight 1

    ::hv3::button $O(win).command -textvar ${fd}(fdgui.buttontext) -width 7
    set O(myButton) $O(win).command

    grid $f $spacer $O(win).command -sticky nsew
    grid columnconfigure $O(win) 0 -weight 1

    $f configure -relief sunken -borderwidth 1

    eval $me configure $args
  }

  proc destroy {me} {
    array unset $me
    rename $me {}
  }

}
::hv3::make_constructor ::hv3::fdgui


# ::hv3::downloadmanager
#
# SYNOPSIS
#
#     set obj [::hv3::downloadmanager %AUTO%]
#
#     $obj show
#     $obj manage FILE-DOWNLOAD
#
#     destroy $obj
#
namespace eval ::hv3::downloadmanager {

  proc new {me} {
    upvar #0 $me O

    # The set of ::hv3::filedownload objects.
    set O(downloads) [list]

    # All ::hv3::hv3 widgets in the system that (may) be 
    # displaying the downloads GUI.
    set O(hv3_list) [list]
  }
  proc destroy {me} {
    unset $me
    rename $me ""
  }

  proc manage {me filedownload} {
    upvar #0 $me O
    set O(downloads) [linsert $O(downloads) 0 $filedownload]
    $me ReloadAllGuis
  }

  proc ReloadAllGuis {me} {
    upvar #0 $me O
    $me CheckGuiList
    foreach hv3 $O(hv3_list) {
      $hv3 goto home://downloads/ -cachecontrol no-cache -nosave
    }
  }

  # This is a helper proc for method [savehandle] to extract any
  # filename-parameter from the value of an HTTP Content-Disposition
  # header. If one exists, the value of the filename parameter is
  # returned. Otherwise, an empty string.
  # 
  # Refer to RFC1806 for the complete format of a Content-Disposition 
  # header. An example is:
  #
  #     {inline ; filename="src.tar.gz"}
  #
  proc ParseContentDisposition {value} {
    set tokens [::hv3::string::tokenise $value]

    set filename ""
    for {set ii 0} {$ii < [llength $tokens]} {incr ii} {
      set t [lindex $tokens $ii]
      set t2 [lindex $tokens [expr $ii+1]]

      if {[string match -nocase $t "filename"] && $t2 eq "="} {
        set filename [lindex $tokens [expr $ii+2]]
        set filename [::hv3::string::dequote $filename]
        break
      }
    }

    return $filename
  }

  # Activate the download manager to save the resource targeted by the
  # ::hv3::request passed as an argument ($handle) to the local 
  # file-system. It is the responsbility of the caller to configure 
  # the download-handle and pass it to the protocol object. The second
  # argument, $data, contains an initial segment of the resource that has
  # already been downloaded. 
  #
  proc savehandle {me protocol handle data {isFinished 0}} {
    upvar #0 $me O

    # Remove the handle from the protocol's accounting system.
    if {$protocol ne ""} {
      $protocol FinishRequest $handle
    }

    # Create a GUI to handle this download
    set dler [::hv3::filedownload %AUTO%                   \
        -source    [$handle cget -uri]                     \
        -cancelcmd [list catch [list $handle destroy]]     \
    ]
    ::hv3::the_download_manager show

    # Redirect the -incrscript and -finscript commands to the download GUI.
    $handle configure -finscript  [list $dler finish $handle]
    $handle configure -incrscript [list $dler append $handle]

    $dler append $handle [$handle rawdata]
    $handle set_rawmode

    # Figure out a default file-name to suggest to the user. This
    # is one of the following (in order of preference):
    #
    # 1. The "filename" field from a Content-Disposition header. The
    #    content disposition header is described in RFC 1806 (and 
    #    later RFC 2183). A Content-Disposition header looks like
    #    this:
    #
    # 2. By extracting the tail of the URI.
    #
    set suggested ""
    foreach {key value} [$handle cget -header] {
      if {[string equal -nocase $key Content-Disposition]} {
        set suggested [ParseContentDisposition $value]
      }
    }
    if {$suggested eq ""} {
      regexp {/([^/]*)$} [$handle cget -uri] -> suggested
    }

    # Pop up a GUI to select a "Save as..." filename. Schedule this as 
    # a background job to avoid any recursive entry to our event handles.
    set cmd [subst -nocommands {
      $dler set_destination [file normal [
          tk_getSaveFile -initialfile {$suggested}
      ]] $isFinished
    }]
    after idle $cmd
  }

  # Prune the list of hv3 widgets displaying the download GUI to 
  # those that actually are.
  proc CheckGuiList {me } {
    upvar #0 $me O
    # Make sure the list of GUI's is up to date.
    set newlist [list]
    foreach hv3 $O(hv3_list) {
      if {[info commands $hv3] eq ""} continue
      set loc [$hv3 location]
      if {[string match home://downloads* $loc] && [$hv3 pending] == 0} {
        lappend newlist $hv3
      } 
    }
    set O(hv3_list) $newlist
  }

  proc UpdateGui {me hv3} {
    upvar #0 $me O
# puts "UPDATE $fdownload"

    foreach node [$hv3 html search .fdgui] {
      set filedownload [$node attr id]
      set fdgui [$node replace]
      if {$fdgui eq ""} {
        set fdgui [::hv3::create_widget_name $node]
        ::hv3::fdgui $fdgui $filedownload \
            -command [list $me Pressbutton $filedownload]
        $node replace $fdgui -deletecmd [list destroy $fdgui]
      }
    }
  }

  proc Pressbutton {me dl} {
    upvar #0 $me O
    set idx [lsearch $O(downloads) $dl]
    set O(downloads) [lreplace $O(downloads) $idx $idx]
    catch {
      if {[$dl state] ne "Finished"} {
        $dl Cancel
      }
      $dl destroy
    }
    $me ReloadAllGuis
  }

  proc request {me handle} {
    upvar #0 $me O
    set hv3 [$handle cget -hv3]

    set document {
      <head>
        <title>Downloads</title>
        <style>
          .fdgui { width:90%; margin: 1em auto; }
          body   { background-color: #c3c3c3 }
        </style>
        </head>
        <body>
          <h1 align=center>Downloads</h1>
    }
    foreach download $O(downloads) {
      append document "<div class=\"fdgui\" id=\"$download\"></div>"
    }
    if {[llength $O(downloads)] == 0} {
      append document "<div align=center><i>There are currently no downloads."
    }

    if {[lsearch $O(hv3_list) $hv3] < 0} {
      lappend O(hv3_list) $hv3
    }

    $handle append $document
    $handle finish
    after idle [list $me UpdateGui $hv3]
  }

  proc show {me } {
    upvar #0 $me O
    $me CheckGuiList
    if {[llength $O(hv3_list)] > 0} {
      set hv3 [lindex $O(hv3_list) 0]
      set win [winfo parent [$hv3 win]]
      .notebook select $win
    } else {
      .notebook add home://downloads/
    }
  }
}
::hv3::make_constructor ::hv3::downloadmanager

proc ::hv3::download_scheme_init {hv3 protocol} {
  $protocol schemehandler download [
    list ::hv3::the_download_manager request
  ]
}
