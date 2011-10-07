namespace eval hv3 { set {version($Id$)} 1 }

::hv3::dom2::stateless Document {
  %NODE%
  %NODE_PROTOTYPE%
  %DOCUMENT%
}

#-------------------------------------------------------------------------
# ::hv3::dom::XMLHttpRequest
#
#     Hv3 will eventually feature a fully-featured XMLHttpRequest object,
#     similar to that described here:
#     
#         http://www.w3.org/TR/XMLHttpRequest/
#
#
::hv3::dom2::stateless XMLHttpRequest {

  # Magical state array. And the matching destructor. The 
  # "constructor" for the Tcl side of the XMLHttpRequest is the
  # [::hv3::dom::newXMLHttpRequest] proc.
  #
  dom_parameter myStateArray

  dom_finalize { 

    # Clean up the download handle, if it exists.
    if {$state(downloadHandle) ne ""} {
      $state(downloadHandle) destroy
    }

    if {$state(xml) ne ""} {
      destroy $state(xml)
    }

    # Clean up the Tcl state for this XMLHttpRequest.
    array unset state 
  }

  # The read-only "readyState" attribute.
  #
  dom_get readyState {
    switch -exact -- $state(readyState) {
      Uninitialized {list number 0}
      Open          {list number 1}
      Sent          {list number 2}
      Receiving     {list number 3}
      Loaded        {list number 4}
      default       {error "Bad myReadyState value: $myReadyState"}
    }
  }

  # The responseXML attribute is supposed to return a Document object
  # created by parsing the retrieved data. But hv3 doesn't support
  # script creation of Document objects at this time, so leave
  # this as [dom_todo].
  dom_get responseXML {
    if {$state(xml) eq "" && $state(readyState) eq "Loaded"} {
      set state(xml) [html .xml_[string map {: _} $myStateArray]]

      $state(xml) configure -parsemode xml -defaultstyle ""
      $state(xml) parse -final $state(responseText)
    }
    if {$state(xml) ne ""} {
      return [list object [list ::hv3::DOM::Document $myDom $state(xml)]]
    }
    return null
  }

  dom_get responseText {list string $state(responseText)}
  dom_get status       {list number $state(status)}
  dom_get statusText   {list string $state(statusText)}

  dom_call open {THIS 
    method 
    uri 
    {async    {boolean false}}
    {user     null}
    {password null}
  } {

    # If there was already a download-handle, destroy it.
    if {$state(downloadHandle) ne ""} {
      $state(downloadHandle) destroy
    }
    set state(downloadHandle) [::hv3::request %AUTO%]

    # Configure the callback scripts "-finscript" and "-incrscript".
    #
    set fin  [list ::hv3::DOM::XMLHttpRequest_Finished $myDom $myStateArray]
    set incr [list ::hv3::DOM::XMLHttpRequest_Incr $myDom $myStateArray]
    $state(downloadHandle) configure -finscript $fin -incrscript $incr
    
    # Check the $method argument. Hv3 only supports GET and POST.
    # Anything that is not a POST is sent as a GET.
    set state(method) GET
    if {[string equal -nocase [lindex $method 1] POST]} {
      set state(method) POST
    }

    # Configure the download-handle with the URI to access.
    set rel [lindex $uri 1]
    set fulluri [$state(hv3) resolve_uri $rel]
    $state(downloadHandle) configure -uri $fulluri

    # Set the asynchronous flag.
    set state(async) [lindex $async 1]
    if {$state(async) eq "" || ![string is boolean $state(async)]} {
      set state(async) false
    }
    
    # Reset the other state elements.
    XMLHttpRequest_SetState $myDom $myStateArray Open
    set state(responseText)   ""
    set state(status)         0
    set state(statusText)     ""
  }

  # Set a request header.
  #
  dom_call -string setRequestHeader {THIS header value} {
    if {$state(readyState) ne "Open"} {
      error "INVALID_STATE_ERR"
    }
  }

  # Send the request. According to the w3c spec, this method can
  # be called with one of three signatures:
  #
  #     void               send();
  #     void               send(in DOMString data);
  #     void               send(in Document data);
  #
  # Hv3 does not support the third option.
  #
  dom_call send {THIS args} {
    if {$state(readyState) ne "Open"} {
      error "INVALID_STATE_ERR"
    }
    if {[llength $args]>0 && $state(method) eq "POST"} {
#puts "Send XMLHttpRequest $myStateArray data=[lindex $args 0 1]"
      $state(downloadHandle) configure -postdata [lindex $args 0 1]
    }
    $state(hv3) makerequest $state(downloadHandle)
    XMLHttpRequest_SetState $myDom $myStateArray Sent
    list
  }

  # Abort the request.
  #
  dom_call_todo abort

  # Retrieve all the HTTP headers from the response.
  #
  dom_call getAllResponseHeaders {THIS} {
    puts "TODO: XMLHttpRequestEvent.getAllResponseHeaders"
  }

  # Retrieve a named HTTP header from the response.
  #
  dom_call -string getResponseHeader {THIS header} {
    puts "TODO: XMLHttpRequestEvent.getResponseHeader"
  }

  #--------------------------------------------------------------
  # End of W3C interface. Below this point is non-standard stuff.
}

::hv3::dom2::stateless XMLHttpRequestEvent {
  dom_parameter myXMLHttpRequest

  # Constants for Event.eventPhase (Definition group PhaseType)
  #
  dom_get CAPTURING_PHASE { list number 1 }
  dom_get AT_TARGET       { list number 2 }
  dom_get BUBBLING_PHASE  { list number 3 }

  # Read-only attributes to access the values set by initEvent().
  #
  dom_get type          { list string "readystatechange" }
  dom_get bubbles       { list boolean 0 }
  dom_get cancelable    { list boolean 0 }

  # TODO: Timestamp is supposed to return a timestamp in milliseconds
  # from the epoch. But the DOM spec notes that this information is not
  # available on all systems, in which case the property should return 0. 
  #
  dom_get timestamp  { list number 0 }

  dom_call_todo initEvent
}

namespace eval ::hv3::DOM {

  # Called as the ::hv3::request -incrscript callback for an 
  # XMLHttpRequest.
  #
  proc XMLHttpRequest_Incr {dom statevar data} {
    upvar #0 $statevar state

    # Assume success...
    set state(status)         200
    set state(statusText)     "OK"
    append state(responseText)   $data

    XMLHttpRequest_SetState $dom $statevar Receiving
  }

  # Called as the ::hv3::request -finscript callback for an 
  # XMLHttpRequest.
  #
  proc XMLHttpRequest_Finished {dom statevar data} {
    upvar #0 $statevar state

    # Assume success...
    set    state(status)         200
    set    state(statusText)     "OK"
    append state(responseText)   $data

    XMLHttpRequest_SetState $dom $statevar Receiving
    $state(downloadHandle) destroy
    set state(downloadHandle) ""
    XMLHttpRequest_SetState $dom $statevar Loaded
  }

  proc XMLHttpRequest_SetState {dom statevar newState} {
    upvar #0 $statevar state
    if {$state(readyState) eq $newState} return

    set state(readyState) $newState
    set uri ""
    catch { set uri [$state(downloadHandle) cget -uri] }

    # The value of XMLHttpRequest.readyState has changed, so fire
    # a "readystatechange" event.
    #
    set this  [list ::hv3::DOM::XMLHttpRequest $dom $statevar]
    set event [list                             \
      CAPTURING_PHASE {number 1}                \
      AT_TARGET       {number 2}                \
      BUBBLING_PHASE  {number 3}                \
      type            {string readystatechange} \
      bubbles         {boolean 0}               \
      cancelable      {boolean 0}               \
      timestamp       {number 0}                \
    ]

    set rc [catch {[$dom see] dispatch $this $event} msg]

    # If an error occured, log it in the debugging window.
    #
    if {$rc} {
      puts $msg
      set objtype [lindex $js_obj 0]
      set name [string map {blob error} [$myDom NewFilename]]
      $myDom Log "XMLHttpRequest readystatechange" $name "event" $rc $msg
    }
  }
}

set ::hv3::dom::next_xmlhttp_id 0
proc ::hv3::dom::newXMLHttpRequest {dom hv3} {

  # Name of the state-array for the new object.
  set statevar "::hv3::DOM::xmlhttprequest_[incr ::hv3::dom::next_xmlhttp_id]"
  upvar #0 $statevar state

  # Intialize the new object state.
  set state(async)          0
  set state(downloadHandle) ""
  set state(method)         "GET"
  set state(readyState)     "Uninitialized"
  set state(responseText)   ""
  set state(status)         0
  set state(statusText)     ""
  set state(hv3)            $hv3

  set state(xml)            ""

  list object [list ::hv3::DOM::XMLHttpRequest $dom $statevar]
}

