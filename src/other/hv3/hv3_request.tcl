catch {namespace eval hv3 { set {version($Id$)} 1 }}

#--------------------------------------------------------------------------
# This file contains the implementation of two types used by hv3:
#
#     ::hv3::request
#

#--------------------------------------------------------------------------
# Class ::hv3::request
#
#     Instances of this class are used to interface between the protocol
#     implementation and the hv3 widget.
#
# OVERVIEW:
#
# HOW CHARSETS ARE HANDLED:
#
#     The protocol implementation (the thing that calls [$download append] 
#     and [$download finish]) passes binary data to this object. This
#     object converts the binary data to utf-8 text, based on the encoding
#     assigned to the request. An encoding may be assigned either by an
#     http header or a <meta> tag.
#
#     Assuming the source of the data is http (or https), then the
#     encoding may be specified by way of a Content-Type HTTP header.
#     In this case, when the protocol configures the -header option
#     (which it does before calling [append] for the first time) the 
#     -encoding option will be automatically set.
#
#
# OPTIONS:
#
#     The following options are set only by the requestor (the Hv3 widget)
#     for the protocol to use as request parameters:
#
#       -cachecontrol
#       -uri
#       -postdata
#       -requestheader
#       -enctype
#       -encoding
#
#     This is set by the requestor also to show the origin of the request:
#
#       -hv3
#
#     These are set by the requestor before the request is made to 
#     configure callbacks invoked by this object when requested data 
#     is available:
#    
#       -incrscript
#       -finscript
#
#     This is initially set by the requestor. It may be modified by the
#     protocol implementation before the first invocation of -incrscript
#     or -finscript is made.
#
#       -mimetype
#
#     The protocol implementation also sets:
#
#       -header
#       -expectedsize
#
# METHODS:
#
#     Methods used by the protocol implementation:
#
#         append DATA
#         finish
#         fail
#         authority         (return the authority part of the -uri option)
#
#     finish_hook SCRIPT
#         Configure the object with a script to be invoked just before
#         the object is about to be destroyed. If more than one of
#         these is configured, then the scripts are called in the
#         same order as they are configured in (i.e. most recently
#         configured is invoked last).
#
#     reference
#     release
#
#     data
#     encoding
#

namespace eval ::hv3::request {

  proc new {me args} {

    upvar $me O

    # The requestor (i.e. the creator of the ::hv3::request object) sets the
    # following configuration options. The protocol implementation may set the
    # -mimetype option before returning.
    #
    # The -cachecontrol option may be set to the following values:
    #
    #     * normal             (try to be clever about caching)
    #     * no-cache           (never return cached resources)
    #     * relax-transparency (return cached resources even if stale)
    #
    set O(-cachecontrol) normal
    set O(-uri) ""
    set O(-postdata) ""
    set O(-mimetype) ""
    set O(-enctype) ""

    set O(-cacheable) 0

    # The hv3 widget that issued this request. This is used
    # (a) to notify destruction of root request,
    # (b) by the handler for home:// uris and
    # (c) to call [$myHtml reset] in restartCallback.
    #
    set O(-hv3) ""

    # The protocol implementation sets this option to contain the 
    # HTTP header (or it's equivalent). The format is a serialised array.
    # Example:
    # 
    #     {Set-Cookie safe-search=on Location http://www.google.com}
    #
    # The following http-header types are handled locally by the 
    # configure-header method, as soon as the -header option is set:
    #
    #     Set-Cookie         (Call ::hv3::the_cookie_manager method)
    #     Content-Type       (Set the -mimetype option)
    #     Content-Length     (Set the -expectedsize option)
    #
    set O(-header) ""
  
    set O(-requestheader) ""
  
    # Expected size of the resource being requested. This is used
    # for displaying a progress bar when saving remote resources
    # to the local filesystem (aka downloadin').
    #
    set O(-expectedsize) ""
  
    # Callbacks configured by the requestor.
    #
    set O(-incrscript) ""
    set O(-finscript) ""
  
    # This -encoding option is used to specify explicit conversion of
    # incoming http/file data.
    # When this option is set, [http::geturl -binary] is used.
    # Then [$self append] will call [encoding convertfrom].
    #
    # See also [encoding] and [suggestedEncoding] methods.
    #
    set O(-encoding) ""
  
    # True if the -encoding option has been set by the transport layer. 
    # If this is true, then any encoding specified via a <meta> element
    # in the main document is ignored.
    #
    set O(-hastransportencoding) 0

    # END OF OPTIONS
    #----------------------------

    set O(chunksize) 2048
  
    # The binary data returned by the protocol implementation is 
    # accumulated in this variable.
    set O(myRaw) {}
    set O(myRawMode) 0
  
    # If this variable is non-zero, then the first $myRawPos bytes of
    # $myRaw have already been passed to Hv3 via the -incrscript 
    # callback.
    set O(myRawPos) 0
  
    # These objects are referenced counted. Initially the reference count
    # is 1. It is increased by calls to the [reference] method and decreased
    # by the [release] method. The object is deleted when the ref-count 
    # reaches zero.
    set O(myRefCount) 1
  
    set O(myIsText) 1; # Whether mimetype is text/* or not.
  
    # Make sure finish is processed only once.
    set O(myIsFinished) 0
  
    # Destroy-hook scripts configured using the [finish_hook] method.
    set O(myFinishHookList) [list]

    set O(myDestroying) 0

    eval configure $me $args
  }

  proc destroy {me} {
    upvar $me O
    set O(myDestroying) 1
    foreach hook $O(myFinishHookList) {
      eval $hook 
    }
    rename $me {}
    array unset $me
  }

  proc data {me} {
    upvar $me O
    set raw [string range $O(myRaw) 0 [expr {$O(myRawPos)-1}]]
    if {$O(myIsText)} {
      return [::encoding convertfrom [encoding $me] $raw]
    }
    return $raw
  }
  proc rawdata {me} {
    upvar $me O
    return $O(myRaw)
  }
  proc set_rawmode {me} {
    upvar $me O
    set O(myRawMode) 1
    set O(myRaw) ""
  }

  # Increment the object refcount.
  #
  proc reference {me} {
    upvar $me O
    incr O(myRefCount)
  }

  # Decrement the object refcount.
  #
  proc release {me} {
    upvar $me O
    incr O(myRefCount) -1
    if {$O(myRefCount) == 0} {
      $me destroy
    }
  }

  # Add a script to be called just before the object is destroyed. See
  # description above.
  #
  proc finish_hook {me script} {
    upvar $me O
    lappend O(myFinishHookList) $script
  }

  # This method is called each time the -header option is set. This
  # is where the locally handled HTTP headers (see comments above the
  # -header option) are handled.
  #
  proc configure-header {me} {
    upvar $me O
    foreach {name value} $O(-header) {
      switch -- [string tolower $name] {
        set-cookie {
          catch {
            ::hv3::the_cookie_manager SetCookie $O(-uri) $value
          }
        }
        content-type {
          set parsed [hv3::string::parseContentType $value]
          foreach {major minor charset} $parsed break
          set O(-mimetype) $major/$minor
          if {$charset ne ""} {
            set O(-hastransportencoding) 1
            set O(-encoding) [::hv3::encoding_resolve $charset]
          }
        }
        content-length {
          set O(-expectedsize) $value
        }
      }
    }
  }

  proc configure-mimetype {me} {
    upvar $me O
    set O(myIsText) [string match text* $O(-mimetype)]
  }

  proc configure-encoding {me} {
    upvar $me O
    set O(-encoding) [::hv3::encoding_resolve $O(-encoding)]
  }

  # Return the "authority" part of the URI configured as the -uri option.
  #
  proc authority {me} {
    upvar $me O
    set obj [::tkhtml::uri $O(-uri)]
    set authority [$obj authority]
    $obj destroy
    return $authority
  }

  # Interface for returning data.
  proc append {me raw} {
    upvar $me O

    if {$O(myDestroying)} {return}
    if {$O(myRawMode)} {
      eval [linsert $O(-incrscript) end $raw]
      return
    }

    ::append O(myRaw) $raw

    if {$O(-incrscript) != ""} {
      # There is an -incrscript callback configured. If enough data is 
      # available, invoke it.

      set nLast 0
      foreach zWhite [list " " "\n" "\t"] {
        set n [string last $zWhite $O(myRaw)]
        if {$n>$nLast} {set nLast $n ; break}
      }
      set nAvailable [expr {$nLast-$O(myRawPos)}]
      if {$nAvailable > $O(chunksize)} {

        set zDecoded [string range $O(myRaw) $O(myRawPos) $nLast]
        if {$O(myIsText)} {
          set zDecoded [::encoding convertfrom [encoding $me] $zDecoded]
        }
        set O(myRawPos) [expr {$nLast+1}]
        if {$O(chunksize) < 30000} {
          set O(chunksize) [expr $O(chunksize) * 2]
        }

        eval [linsert $O(-incrscript) end $zDecoded] 
      }
    }
  }

  # Called after all data has been passed to [append].
  #
  proc finish {me {raw ""}} {
    upvar $me O

    if {$O(myDestroying)} {return}
    if {$O(myIsFinished)} {error "finish called twice on $me"}
    set O(myIsFinished) 1

    if {$O(myRawMode)} {
      foreach hook $O(myFinishHookList) {
        eval $hook
      }
      eval [linsert $O(-finscript) end $raw]
      return
    }

    ::append O(myRaw) $raw

    set zDecoded [string range $O(myRaw) $O(myRawPos) end]
    if {$O(myIsText)} {
      set zDecoded [::encoding convertfrom [encoding $me] $zDecoded]
    }

    foreach hook $O(myFinishHookList) {
      eval $hook
    }
    set O(myFinishHookList) [list]
    set O(myRawPos) [string length $O(myRaw)]
    eval [linsert $O(-finscript) end $zDecoded] 
  }

  proc isFinished {me} {
    upvar $me O
    set O(myIsFinished)
  }

  proc fail {me} {
    upvar $me O
    # TODO: Need to do something here...
    puts FAIL
  }

  proc encoding {me} {
    upvar $me O
    set ret $O(-encoding)
    if {$ret eq ""} {set ret [::encoding system]}
    return $ret
  }
}

::hv3::make_constructor ::hv3::request

