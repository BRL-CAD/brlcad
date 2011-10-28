namespace eval hv3 { set {version($Id$)} 1 }

#
# The code in this file handles <object> elements for the hv3 mini-browser.
#
# This code is part of the hv3 widget. Therefore it may use internal hv3
# interfaces.
#

# Given a file extension, return the MIME type to be used for the file.
# This should only be used if the document author does not specify or 
# imply a MIME type as part of the document.
#
proc hv3_ext_to_mime {extension} {

  switch -- [string tolower $extension] {
    html {set mime text/html}
    htm  {set mime text/html}
    tcl  {set mime application/x-tcl}
    png  {set mime image/png}

    default {
      set mime ""
    }
  }

  return $mime
}

# $node is an <object> element. This proc collects values from any <param>
# elements that are immediate children of the <object> and returns them as
# a list. The list consists of one element for each specified parameter, in 
# the following form:
#
#     {name value valuetype}
#
# Where valuetype may be "data", "object" or a MIME-type.
#
proc hv3_object_getparams {node} {
  return [list]
}

# A node-handler callback for the following elements:
#
#     <object> 
#     <embed>
#     <iframe> (todo)
#
proc hv3_object_handler {hv3 node} {

  # This implementation only supports the following object element 
  # attributes. Figure these out and store them in local variables.
  #
  #     * data
  #     * type
  #     * standby
  #
  # If the element identified by $node is something other than an 
  # <object>, values are mapped from the element specific attributes
  # to their generic <object> equivalents.
  #
  switch -- [$node tag] {
    object {
      set params [hv3_object_getparams $node]
      set standby [$node attr -default "" standby]
      set data [$node attr -default "" data]
      set type [$node attr -default "" type]
    }
    embed {
      set standby ""
      set data [$node attr -default "" src]
      set type [$node attr -default "" type]
      set params [$node attr]
    }
  }

  if {$data eq ""} {
    # If there is no URI specified as the "data" attribute, we're out of 
    # luck. This object will not be handled as an object. Instead, the
    # content will be rendered.
    return
  }

  # In case the data attribute was a relative URI, resolve it.
  set data [$hv3 resolve_uri $data]

  if {$type eq ""} {
    # If there is no expected MIME type specified, try to guess one from
    # the extension of the target URI. Any MIME type determined here will
    # likely be overridden by the MIME type returned by the HTTP server
    # anyway, so it's not such a big deal.
    set extension ""
    regexp {[^./]*$} $data extension
    set type [hv3_ext_to_mime $extension]
  }

  # If a "standby" attribute was specified, create a label widget to display
  # the standby text while the user waits for the object content to download.
  #
  # If no "standby" text was explicitly specified, let hv3 render the content
  # of the object for the time being.
  if {$standby ne ""} {
    set html [$hv3 html]
    set standby_widget "${html}.[string map {: _} $node]_standby"
    label $standby_widget -text $standby
    $node replace $standby_widget
  }

  # Download the data for this object. To do this, create an ::hv3::request
  # object and pass it to the hv3 widget's -getcmd script.
  set handle [::hv3::request %AUTO%]
  $handle configure \
      -uri       $data                                                       \
      -finscript  [list hv3_object_data_handler $hv3 $node $params $handle]  \
      -mimetype  $type
  $hv3 makerequest $handle
}

proc hv3_collect_data {data handle} {
  $handle append $data
  $handle finish
}

proc hv3_object_data_handler {hv3 node params handle data} {
  if {$data ne ""} {
    set mimetype [$handle cget -mimetype]
  
    switch -glob -- $mimetype {
  
      application/x-tcl {
        set html [$hv3 html]
        set tclet "${html}.[string map {: _} $node]_tclet"
        ::hv3::tclet $tclet $params $data
        $node replace $tclet -configurecmd [list $tclet configurecmd]
      }
  
      text/html {
if 0 {
        set html [$hv3 html]
        set htmlet "${html}.[string map {: _} $node]_tclet"
        ::hv3::hv3 $htmlet
        $htmlet configure -requestcmd [list hv3_collect_data $data]
        $htmlet goto [$handle cget -uri]
        $htmlet configure -requestcmd       [$hv3 cget -requestcmd]
        $node replace $htmlet
}
      }
  
      image/* {
        set img [image create photo -data $data]
        $node replace $img -deletecmd [list image delete $img]
        $node override [list -tkhtml-replacement-image url(replace:$img)]
      }
  
      default { $node replace "" }
    }
  } else {
    $node replace ""
  }

  $handle destroy
}

snit::widget ::hv3::tclet {
  variable myInterp ""
  variable myFrame ""

  constructor {params data} {
    set myFrame [frame ${self}.tclet_frame -container 1]
    pack $myFrame -expand 1 -fill both

    # Create a safe interpreter with Tk available.
    set myInterp [::safe::interpCreate]
    ::safe::loadTk $myInterp -use $myFrame

    # Set up the partial emulated Tcl plugin environment based on the 
    # documentation for the real thing at:
    #
    #     http://www.tcl.tk/software/plugin/
    #
    interp eval $myInterp [list array set ::embed_args $params]
    interp eval $myInterp [list set plugin(patchLevel) $::tcl_patchLevel]
    interp eval $myInterp [list set plugin(release) 20060525]
    interp eval $myInterp [list set plugin(version) 3]

    set rc [catch {interp eval $myInterp $data} msg]
    if {$rc} {
      tk_messageBox -message $msg -type ok
    }
  }

  method configurecmd {values} {
      array set v $values
      catch { $myFrame configure -width $v(width) }
      catch { $myFrame configure -height $v(height) }
  }

  destructor {
    if {$myInterp ne ""} {interp delete $myInterp}
  }
}

