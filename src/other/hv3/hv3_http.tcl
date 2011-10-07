namespace eval hv3 { set {version($Id$)} 1 }

#
# This file contains implementations of the -requestcmd script used with 
# the hv3 widget for the browser. Supported functions are:
#
#     * http:// (including cookies)
#     * file:// (code for this is now in hv3_file.tcl)
#     * data://
#     * blank://
#     * https:// (if the "tls" package is available)
#

package require snit
package require Tk
package require http 2.7
catch { package require tls }

source [sourcefile hv3_file.tcl]

#
# ::hv3::protocol
#
#     Connect hv3 to the outside world via download-handle objects.
#
# Synopsis:
#
#     set protocol [::hv3::protocol %AUTO%]
#
#     $protocol requestcmd DOWNLOAD-HANDLE
#
#     $protocol schemehandler scheme handler
#
#     $protocol destroy
#
namespace eval ::hv3::protocol {

  proc new {me args} { 
    upvar $me O

    # Lists of waiting and in-progress http URI download-handles.
    set O(myWaitingHandles)    [list]
    set O(myInProgressHandles) [list]

    set O(myQueue)             [list]

    # If not set to an empty string, contains the name of a global
    # variable to set to a short string describing the state of
    # the object. The string is always of the form:
    #
    #     "X1 waiting, X2 in progress"
    #
    # where X1 and X2 are integers. An http request is said to be "waiting"
    # until the header identifying the mimetype is received, and "in progress"
    # from that point on until the resource has been completely retrieved.
    #
    set O(-statusvar) ""
    set O(-relaxtransparency) 0

    # Instance of ::hv3::cookiemanager. Right now this is a global object.
    # But this may change in the future. Hence this variable.
    #
    set O(myCookieManager) ""

    set O(myBytesExpected) 0
    set O(myBytesReceived) 0

    set O(myGui) ""

    eval configure $me $args

    # It used to be that each ::hv3::protocol object would create it's
    # own cookie-manager database. This has now changed so that the
    # whole application (all browser tabs) use a single cookies 
    # database. The net effect is that you can log in to a web site
    # in one tab and then continue in another.
    #
    # The global cookie-manager object is named "::hv3::the_cookie_manager".
    #
    set O(myCookieManager) ::hv3::the_cookie_manager
    if {[info commands $O(myCookieManager)] eq ""} {
      ::hv3::cookiemanager $O(myCookieManager)
    }

    # Register the 4 basic types of URIs the ::hv3::protocol code knows about.
    schemehandler $me file  ::hv3::request_file
    schemehandler $me http  [list $me request_http]
    schemehandler $me data  [list $me request_data]
    schemehandler $me blank [list $me request_blank]
    schemehandler $me about [list $me request_blank]

    # If the tls package is loaded, we can also support https.
    if {[info commands ::tls::socket] ne ""} {
      schemehandler $me https [list $me request_https]
      ::http::register https 443 ::hv3::protocol::SSocket
    }

    # Configure the Tcl http package to pretend to be Gecko.
    # ::http::config -useragent {Mozilla/5.0 Gecko/20050513}
    ::http::config -useragent {Mozilla/5.1 (X11; U; Linux i686; en-US; rv:1.8.0.3) Gecko/20060425 SUSE/1.5.0.3-7 Hv3/alpha}
    set ::http::defaultCharset utf-8
  }

  proc destroy {me} { 
    # Nothing to do. We used to destroy the $myCookieManager object here,
    # but that object is now global and exists for the lifetime of the
    # application.
    array unset $me
    rename $me {}
  }

  # Register a custom scheme handler command (like "home:").
  proc schemehandler {me scheme handler} {
    upvar $me O
    set O(scheme.$scheme) $handler
  }

  # This method is invoked as the -requestcmd script of an hv3 widget
  proc requestcmd {me downloadHandle} {
    upvar $me O

    # Extract the URI scheme to figure out what kind of URI we are
    # dealing with. Currently supported are "file" and "http" (courtesy 
    # Tcl built-in http package).
    set uri_obj [::tkhtml::uri [$downloadHandle cget -uri]]
    set uri_scheme [$uri_obj scheme]
    $uri_obj destroy

    # Fold the scheme to lower-case. Should ::tkhtml::uri have already 
    # done this?
    set uri_scheme [string tolower $uri_scheme]

    # Execute the scheme-handler, or raise an error if no scheme-handler
    # can be found.
    if {[info exists O(scheme.$uri_scheme)]} {
      eval [concat $O(scheme.$uri_scheme) $downloadHandle]
    } else {
      error "Unknown URI scheme: \"$uri_scheme\""
    }
  }

  # Handle an http:// URI.
  #
  proc request_http {me downloadHandle} {
    upvar $me O
    #puts "REQUEST: [$downloadHandle cget -uri]"

    #$downloadHandle finish
    #return

    set uri       [$downloadHandle cget -uri]
    set postdata  [$downloadHandle cget -postdata]
    set enctype   [$downloadHandle cget -enctype]

    if {[$downloadHandle cget -cachecontrol] eq "relax-transparency" 
     && [$downloadHandle cget -cacheable]
    } {
      if {[::hv3::the_httpcache query $downloadHandle]} {
        return
      }
    }

    # Store the HTTP header containing the cookies in variable $headers
    set headers [$downloadHandle cget -requestheader]
    set cookies [$O(myCookieManager) Cookie $uri]
    if {$cookies ne ""} {
      lappend headers Cookie $cookies
    }

    # TODO: This is ridiculous. The version of cvstrac on tkhtml.tcl.tk
    # does something that confuses polipo into incorrectly caching 
    # pages. And it's too hard right now to fix the root cause, so work
    # around it by explicitly telling polipo not to use cached instances
    # of these pages.
    #
    set handle_cachecontrol [$downloadHandle cget -cachecontrol]
    if {$handle_cachecontrol eq "normal"} {
      if {[string first http://tkhtml.tcl.tk/cvstrac $uri] == 0} {
        set handle_cachecontrol no-cache
      }
    }

    switch -- $handle_cachecontrol {
      relax-transparency {
        lappend headers Cache-Control relax-transparency=1
      }
      no-cache {
        lappend headers Cache-Control no-cache
      }
      default {
      }
    }
    if {$O(-relaxtransparency)} {
      lappend headers Cache-Control relax-transparency=1
    }

    # if {0 || ($::hv3::polipo::g(binary) ne "" 
    #  && $postdata eq "" 
    #  && ![string match -nocase https* $uri])
    # } {
    #   $me AddToWaitingList $downloadHandle
    #   set p [::hv3::polipoclient %AUTO%]
    #   $downloadHandle finish_hook [list $p destroy]
    #   $downloadHandle finish_hook [list $me FinishRequest $downloadHandle]
    #   $p GET $me $downloadHandle
    #   return
    # }

    # Fire off a request via the http package.
    # Always uses -binary mode.
    set geturl [list ::http::geturl $uri                     \
      -command [list $me _DownloadCallback $downloadHandle]  \
      -handler [list $me _AppendCallback $downloadHandle]    \
      -headers $headers                                      \
      -binary 1                                              \
    ]
    if {$postdata ne ""} {
      lappend geturl -query $postdata
      if {$enctype ne ""} {
        lappend geturl -type $enctype
      }
    }

    set token [eval $geturl]
    $me AddToWaitingList $downloadHandle
    $downloadHandle finish_hook [list ::http::reset $token]
#puts "REQUEST $geturl -> $token"
  }

  proc BytesReceived {me downloadHandle nByte} {
    upvar $me O
    set nExpected [$downloadHandle cget -expectedsize]
    if {$nExpected ne ""} {
      incr O(myBytesReceived) $nByte
    }
    $me Updatestatusvar
  }
  proc AddToProgressList {me downloadHandle} {
    upvar $me O
    set i [lsearch $O(myWaitingHandles) $downloadHandle]
    set O(myWaitingHandles) [lreplace $O(myWaitingHandles) $i $i]
    lappend O(myInProgressHandles) $downloadHandle

    set nExpected [$downloadHandle cget -expectedsize]
    if {$nExpected ne ""} {
      incr O(myBytesExpected) $nExpected
    }

    $me Updatestatusvar
  }

  proc AddToWaitingList {me downloadHandle} {
    upvar $me O

    if {[lsearch -exact $O(myWaitingHandles) $downloadHandle] >= 0} return

    # Add this handle the the waiting-handles list. Also add a callback
    # to the -failscript and -finscript of the object so that it 
    # automatically removes itself from our lists (myWaitingHandles and
    # myInProgressHandles) after the retrieval is complete.
    #
    lappend O(myWaitingHandles) $downloadHandle
    $downloadHandle finish_hook [list $me FinishRequest $downloadHandle]
    $me Updatestatusvar
  }

  # The following methods:
  #
  #     [request_https], 
  #     [SSocketReady], 
  #     [SSocketProxyReady], and
  #     [SSocket], 
  #
  # along with the type variable $theWaitingSocket, are part of the
  # https:// support implementation.
  # 
  proc request_https {me downloadHandle} {
    upvar $me O

    set obj [::tkhtml::uri [$downloadHandle cget -uri]]
    set host [$obj authority]
    $obj destroy

    set port 443
    regexp {^(.*):([0123456789]+)$} $host -> host port

    set proxyhost [::http::config -proxyhost]
    set proxyport [::http::config -proxyport]

    AddToWaitingList $me $downloadHandle

    if {$proxyhost eq ""} {
      set fd [socket -async $host $port]
      fileevent $fd writable [list $me SSocketReady $fd $downloadHandle]
    } else {
      set fd [socket $proxyhost $proxyport]
      fconfigure $fd -blocking 0 -buffering full
      puts $fd "CONNECT $host:$port HTTP/1.1"
      puts $fd ""
      flush $fd
      fileevent $fd readable [list $me SSocketProxyReady $fd $downloadHandle]
    }
  }

  proc SSocketReady {me fd downloadHandle} {
    upvar $me O
    ::variable theWaitingSocket

    # There is now a tcp/ip socket connection to the https server ready 
    # to use. Invoke ::tls::import to add an SSL layer to the channel
    # stack. Then call [$me request_http] to format the HTTP request
    # as for a normal http server.
    fileevent $fd writable ""
    fileevent $fd readable ""

    if {[info commands $downloadHandle] eq ""} {
      # This occurs if the download-handle was cancelled by Hv3 while
      # waiting for the SSL connection to be established. 
      close $fd
    } else {
      set theWaitingSocket $fd
      $me request_http $downloadHandle
    }
  }
  proc SSocketProxyReady {me fd downloadHandle} {
    upvar $me O
    fileevent $fd readable ""

    set str [gets $fd line]
    if {$line ne ""} {
      if {! [regexp {^HTTP/.* 200} $line]} {
        puts "ERRRORR!: $line"
        close $fd
        $downloadHandle finish [::hv3::string::htmlize $line]
        return
      } 
      while {[gets $fd r] > 0} {}
      set fd [::tls::import $fd]
      fconfigure $fd -blocking 0

      set cmd [list $me SSocketReady $fd $downloadHandle] 
      SIfHandshake $fd $downloadHandle $cmd
      # $me SSocketReady $fd $downloadHandle
    }
  }
  proc SIfHandshake {fd downloadHandle script} {
    if {[ catch { set done [::tls::handshake $fd] } msg]} {
      $downloadHandle finish [::hv3::string::htmlize $msg]
      return
    }
    if {$done} {
      eval $script
    } else {
      after 100 [list ::hv3::protocol::SIfHandshake $fd $downloadHandle $script]
    }
  }

  # Namespace variable and proc.
  ::variable theWaitingSocket ""
  proc SSocket {host port} {
    ::variable theWaitingSocket
    set ss $theWaitingSocket
    set theWaitingSocket ""
    return $ss
  }

  # End of code for https://
  #-------------------------

  # Handle a data: URI.
  #
  # RFC2397: # http://tools.ietf.org/html/2397
  #
  #    dataurl    := "data:" [ mediatype ] [ ";base64" ] "," data
  #    mediatype  := [ type "/" subtype ] *( ";" parameter )
  #    data       := *urlchar
  #    parameter  := attribute "=" value
  #
  proc request_data {me downloadHandle} {
    upvar $me O

    set uri [$downloadHandle cget -uri]
    set iData [expr [string first , $uri] + 1]

    set data [string range $uri $iData end]
    set header [string range $uri 0 [expr $iData - 1]]

    if {[string match {*;base64} $header]} {
      set bin [::tkhtml::decode -base64 $data]
    } else {
      set bin [::tkhtml::decode $data]
    }

    if {[regexp {^data:/*([^,;]*)} $uri dummy mimetype]} {
        $downloadHandle configure -mimetype $mimetype
    }

    $downloadHandle append $bin
    $downloadHandle finish
  }

  # Namespace proc.
  proc request_blank {me downloadHandle} {
    upvar $me O
    # Special case: blank://
    if {[string first blank: [$downloadHandle cget -uri]] == 0} {
      $downloadHandle append ""
      $downloadHandle finish
      return
    }
  }

  proc FinishRequest {me downloadHandle} {
    upvar $me O

    if {[set idx [lsearch $O(myInProgressHandles) $downloadHandle]] >= 0} {
      set O(myInProgressHandles) [lreplace $O(myInProgressHandles) $idx $idx]
    }
    if {[set idx [lsearch $O(myWaitingHandles) $downloadHandle]] >= 0} {
      set O(myWaitingHandles) [lreplace $O(myWaitingHandles) $idx $idx]
    }
    if {[set idx [lsearch $O(myQueue) $downloadHandle]] >= 0} {
      set O(myQueue) [lreplace $O(myQueue) $idx $idx]
    }
    if {[llength $O(myWaitingHandles)]==0 && [llength $O(myInProgressHandles)]==0} {
      set O(myBytesExpected) 0
      set O(myBytesReceived) 0
    }
    $me Updatestatusvar
  }

  # Update the value of the -statusvar variable, if the -statusvar
  # option is not set to an empty string.
  proc Updatestatusvar {me} {
    upvar $me O

    if {$O(-statusvar) ne ""} {
      set nWait [llength $O(myWaitingHandles)]
      set nProgress [llength $O(myInProgressHandles)]
      if {$nWait > 0 || $nProgress > 0} {
        set f ?
        if {$O(myBytesExpected) > 0} {
          set f [expr {$O(myBytesReceived)*100/$O(myBytesExpected)}]
        }
        set value [list $nWait $nProgress $f]
      } else {
        set value [list]
      }

      uplevel #0 [list set $O(-statusvar) $value]
    }
    catch {$O(myGui) populate}
  }
  
  proc busy {me} {
    upvar $me O
    return [expr [llength $O(myWaitingHandles)] + [llength $O(myInProgressHandles)]]
  }

  # Invoked to set the value of the -statusvar option
  proc configure-statusvar {me} {
    Updatestatusvar $me
  }

  # Invoked when data is available from an http request. Pass the data
  # along to hv3 via the downloadHandle.
  #
  proc _AppendCallback {me downloadHandle socket token} {
    upvar $me O

    upvar \#0 $token state 

    # If this download-handle is still in the myWaitingHandles list,
    # process the http header and move it to the in-progress list.
    if {0 <= [set idx [lsearch $O(myWaitingHandles) $downloadHandle]]} {

      # Remove the entry from myWaitingHandles.
      set O(myWaitingHandles) [lreplace $O(myWaitingHandles) $idx $idx]

      # Copy the HTTP header to the -header option of the download handle.
      $downloadHandle configure -header $state(meta)

      # Add the handle to the myInProgressHandles list and update the
      # status report variable.
      lappend O(myInProgressHandles) $downloadHandle 

      set nExpected [$downloadHandle cget -expectedsize]
      if {$nExpected ne ""} {
        incr O(myBytesExpected) $nExpected
      }
    }

    set data [read $socket]
    set rc [catch [list $downloadHandle append $data] msg]
    if {$rc} { puts "Error: $msg $::errorInfo" }
    set nbytes [string length $data]

    set nExpected [$downloadHandle cget -expectedsize]
    if {$nExpected ne ""} {
      incr O(myBytesReceived) $nbytes
    }

    $me Updatestatusvar

    return $nbytes
  }

  # Invoked when an http request has concluded.
  #
  proc _DownloadCallback {me downloadHandle token} {
    upvar $me O

    if {
      [lsearch $O(myInProgressHandles) $downloadHandle] >= 0 ||
      [lsearch $O(myWaitingHandles) $downloadHandle] >= 0
    } {
      catch {$O(myGui) uri_done [$downloadHandle cget -uri]}
      if {[$downloadHandle cget -cacheable]} {
        ::hv3::the_httpcache add $downloadHandle
      }
    }

    catch { $downloadHandle finish }
    ::http::cleanup $token
  }

  proc debug_cookies {me} {
    upvar $me O
    $O(myCookieManager) debug
  }

  # gui --
  #
  #     This method is called to retrieve the GUI associated with
  #     this protocol implementation. The protocol GUI should be a 
  #     window named $name suitable to [pack] in with the main browser 
  #     window.
  #
  proc gui {me name} {
    upvar $me O
    catch {destroy $O(myGui)}
    ::hv3::protocol_gui $name $me
    set O(myGui) $name
  }

  proc waiting_handles {me} {
    upvar $me O
    return $O(myWaitingHandles)
  }
  proc inprogress_handles {me} {
    upvar $me O
    return $O(myInProgressHandles)
  }
}

::hv3::make_constructor ::hv3::protocol

snit::widget ::hv3::protocol_gui {
  
  variable myProtocol ""
  variable myTimerId ""

  variable myDoneList [list]

  constructor {protocol} {
    set myProtocol $protocol
    ::hv3::scrolled ::hv3::text ${win}.text -propagate 1
    ${win}.text.widget configure -height 2
    pack ${win}.text -expand 1 -fill both
    $self populate
  }

  destructor {
    catch {after cancel $myTimerId}
  }

  method uri_done {uri} {
    lappend myDoneList $uri
    $self populate
  }

  method populate {} {
    set yview [lindex [${win}.text yview] 0]
    ${win}.text delete 0.0 end

    set n 0
    foreach uri $myDoneList {
      ${win}.text insert end [format "%-15s%s\n" DONE $uri]
      incr n
    }
    foreach h [$myProtocol waiting_handles] {
      ${win}.text insert end [format "%-15s%s\n" WAITING [$h cget -uri]] 
      incr n
    }
    foreach h [$myProtocol inprogress_handles] {
      ${win}.text insert end [format "%-15s%s\n" {IN PROGRESS} [$h cget -uri]]
      incr n
    }

    if {$n > 15} {set n 15}
    if {$n > [${win}.text.widget cget -height]} {
      ${win}.text.widget configure -height $n
    }
    ${win}.text yview moveto $yview
  }
}

#-----------------------------------------------------------------------
# Work around a bug in http::Finish
#

# UPDATE: This bug was a leaking file-descriptor. However, it seems to
# have been fixed somewhere around version 2.7. So the [package require]
# at the top of this file has been changed to require at least version
# 2.7 and the workaround code commented out.
#

if 0 {
  # First, make sure the http package is actually loaded. Do this by 
  # invoking ::http::geturl. The call will fail, since the arguments (none)
  # passed to ::http::geturl are invalid.
  catch {::http::geturl}
  
  # Declare a wrapper around ::http::Finish
  proc ::hv3::HttpFinish {token args} {
    upvar 0 $token state
    catch {
      close $state(sock)
      unset state(sock)
    }
    eval [linsert $args 0 ::http::FinishReal $token]
  }
  
  # Install the wrapper.
  rename ::http::Finish ::http::FinishReal
  rename ::hv3::HttpFinish ::http::Finish
}
#-----------------------------------------------------------------------

