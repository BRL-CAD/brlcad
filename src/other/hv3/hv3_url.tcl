###########################################################################
# hv3_url.tcl --
#
#     This file contains code to manipulate and download data from URI's.
#
# Public interface:
#
#     String manipulation: [url_resolve] [url_get]
#     Networking:          [url_fetch] [url_cancel]
#
#     Additionally, the variable ::hv3_url_status is sometimes set to indicate
#     the status of network activities (is suitable for display in web-browser
#     "status" bar).
#

#--------------------------------------------------------------------------
# Global variables section
array unset url_g_scripts
set http_current_socket -1
array unset http_name_cache

array unset ::hv3_url_status_info
set         ::hv3_url_status {}
array unset ::hv3_url_tokens
#--------------------------------------------------------------------------

proc urlSetStatus {} {
    set newval {}
    foreach k [array names ::hv3_url_status_info] {
        append newval "${k}s: [join $::hv3_url_status_info($k) /]  "
    }
    set ::hv3_url_status $newval
}
proc urlStart {type} {
    if {![info exists ::hv3_url_status_info($type)]} {
        set ::hv3_url_status_info($type) [list 0 0]
    } 
    lset ::hv3_url_status_info($type) 1 [
        expr [lindex $::hv3_url_status_info($type) 1] + 1
    ]
    urlSetStatus
}
proc urlFinish {type} {
    lset ::hv3_url_status_info($type) 0 [
        expr [lindex $::hv3_url_status_info($type) 0] + 1
    ]
    urlSetStatus
}

# The url procedures use the following global variables:
#
#      url_g_scripts
#      http_current_socket
#      http_name_cache
#
# And the following internal procedures:
#
#      url_callback
#      http_get_socket
#      http_get_url
#

#--------------------------------------------------------------------------
# urlNormalize --
#
#         urlNormalize PATH
#
#     The argument is expected to be the path component of a URL (i.e. similar
#     to a unix file system path). ".." and "." components are removed and the
#     result returned.
#
proc urlNormalize {path} {
    set ret [list]
    foreach c [split $path /] {
        if {$c == ".."} {
            set ret [lrange $ret 0 end-1]
        } elseif {$c == "."} {
            # Do nothing...
        } else {
            lappend ret $c
        }
    }
    return [join $ret /]
}

#--------------------------------------------------------------------------
# urlSplit --
#
#         urlSplit URL
#
#     Form of URL parsed:
#         <scheme>://<host>:<port>/<path>?<query>#<fragment>
#
proc urlSplit {url} {
    set re_scheme   {((?:[a-z]+:)?)}
    set re_host     {((?://[A-Za-z0-9.]*)?)}
    set re_port     {((?::[0-9]*)?)}
    set re_path     {((?:[^#?]*)?)}
    set re_query    {((?:\?[^?]*)?)}
    set re_fragment {((?:#.*)?)}

    set re "${re_scheme}${re_host}${re_port}${re_path}${re_query}${re_fragment}"

    if {![regexp $re $url X \
            u(scheme) \
            u(host) \
            u(port) \
            u(path) \
            u(query) \
            u(fragment)
    ]} {
        error "Bad URL: $url"
    }

    return [array get u]
}

#--------------------------------------------------------------------------
# url_resolve --
#
#     This command is used to transform a (possibly) relative URL into an
#     absolute URL. Example:
#
#         $ url_resolve http://host.com/dir1/dir2/doc.html ../dir3/doc2.html
#         http://host.com/dir1/dir3/doc2.html
#
#     This is purely a string manipulation procedure.
#
proc url_resolve {baseurl url} {

    array set u [urlSplit $url]
    array set b [urlSplit $baseurl]

    set ret {}
    foreach part [list scheme host port] {
        if {$u($part) != ""} {
            append ret $u($part)
        } else {
            append ret $b($part)
        }
    }

    if {$b(path) == ""} {set b(path) "/"}

    if {[regexp {^/} $u(path)] || $u(host) != ""} {
        set path $u(path)
    } else {
        if {$u(path) == ""} {
            set path $b(path)
        } else {
            regexp {.*/} $b(path) path
            append path $u(path)
        }
    }

    append ret [urlNormalize $path]
    append ret $u(query)
    append ret $u(fragment)

    # puts "$baseurl + $url -> $ret"
    return $ret
}

#--------------------------------------------------------------------------
# url_get --
#
#     This is a high-level string manipulation procedure to extract components
#     from a URL.
#
#         -fragment
#         -prefragment
#         -port
#         -host
#
swproc url_get {url {fragment ""} {prefragment ""} {port ""} {host ""}} {
    array set u [urlSplit $url]

    if {$fragment != ""} {
        uplevel [subst {
            set $fragment "[string range $u(fragment) 1 end]"
        }]
    }

    if {$prefragment != ""} {
        uplevel [subst {
            set $prefragment "$u(scheme)$u(host)$u(port)$u(path)$u(query)"
        }]
    }

    if {$host != ""} {
        uplevel [subst {
            set $host "[string range $u(host) 2 end]"
        }]
    }

    if {$port != ""} {
        uplevel [subst {
            set $port "[string range $u(port) 1 end]"
        }]
    }
}

#--------------------------------------------------------------------------
# cache_init, cache_store, cache_query, cache_fetch --
#
#         cache_init
#         cache_store URL DATA
#         cache_query URL
#         cache_fetch URL
#
#     A tiny API to implement a primitive web cache.
#
proc cache_init {file} {
  sqlite3 dbcache $file
  .html var cache dbcache
  catch {
    [.html var cache] eval {CREATE TABLE cache(url PRIMARY KEY, data BLOB);}
  }
}
proc cache_store {url data} {
  set sql {REPLACE INTO cache(url, data) VALUES($url, $data);}
  [.html var cache] eval $sql
}
proc cache_query {url} {
  set sql {SELECT count(*) FROM cache WHERE url = $url}
  return [[.html var cache] one $sql]
}
proc cache_fetch {url} {
  set sql {SELECT data FROM cache WHERE url = $url}
  return [[.html var cache] one $sql]
}
#
#--------------------------------------------------------------------------

#--------------------------------------------------------------------------
# Enhancement to the http package - asychronous socket connection
#
#     The tcllib http package does not support asynchronous connections
#     (although asynchronous IO is supported after the connection is
#     established). The following code is a hack to work around that.
#
#     Note: Asynchronous dns lookup is required too!
#
set UA "Mozilla/5.0 (compatible; Konqueror/3.3; Linux) (KHTML, like Gecko)"
::http::register http 80 http_get_socket
::http::config -useragent $UA

set http_current_socket -1
array set http_name_cache [list]

proc http_get_socket {args} {
  set ret $::http_current_socket
  set ::http_current_socket -1
  return $ret
}

proc http_socket_ready {url args} {
    set ::hv3_url_tokens([eval [concat ::http::geturl $url $args]]) 1
}

proc http_get_url {url args} {
    url_get $url -port port -host server
    if {$port == ""} {
        set port 80
    } 
  
    if {![info exists ::http_name_cache($server)]} {
        set ::http_name_cache($server) [::tk::htmlresolve $server]
    }
    set server_ip $::http_name_cache($server)

   # set script [concat [list ::http::geturl $url] $args]
    set script [concat [list http_socket_ready $url] $args]
    set s [socket -async $server_ip $port]
    fileevent $s writable [subst -nocommands {
        set ::http_current_socket $s
        $script
        if {[http_get_socket] != -1} {error "assert()"}
    }]
}
#--------------------------------------------------------------------------

#--------------------------------------------------------------------------
# url_fetch --
#
#         url_fetch URL ?-switches ...?
#
#         -script        (default {})
#         -cache         (default {})
#         -type          (default Document)
#         -binary
# 
#     This procedure is used to retrieve remote files. Argument -url is the
#     url to retrieve. When it has been retrieved, the data is appended to
#     the script -script (if any) and the result invoked.
# 
swproc url_fetch {url {script {}} {cache {}} {type Document} {binary 0 1}} {

    # Check the cache before doing anything else. If we can find the data in
    # the cache then invoke the script immediately.
    if {[cache_query $url]} {
        if {$script != ""} {
            set data [cache_fetch $url]
            lappend script $data
            eval $script
        }
        return
    }

  switch -regexp -- $url {

    {^file://} {
      # Handle file:// URLs by loading the contents of the specified file
      # system entry. Invoke any -script directly.
      set rc [catch {
        set fname [string range $url 7 end]
        set f [open $fname]
        if {$binary} {
            fconfigure $f -encoding binary -translation binary
        }
        set data [read $f]
        close $f
        if {$script != ""} {
          lappend script $data
          eval $script
        }
      } msg]
    }

    {^http://} {
      if {0 == [info exists ::url_g_scripts($url)]} {
        set cmd [list url_callback -type $type] 
        set rc [catch {
          http_get_url $url -command $cmd -timeout 120000
        } msg]
        set ::url_g_scripts($url) [list]
        urlStart $type
        gui_log "DOWNLOAD Start: \"$url\""
      }
      if {$script != ""} {
        lappend ::url_g_scripts($url) $script
      }
    }

    default {
      # Any other kind of URL is an error
      set rc 1
      set msg {}
    }
  }
}

# url_cancel --
#
#         url_cancel
#
#     Cancel all currently executing downloads. Do not invoke any -script
#     scripts passed to url_fetch.
#
proc url_cancel {} {
    array unset ::hv3_url_status_info
    urlSetStatus
    foreach k [array names ::hv3_url_tokens] {
        ::http::reset $k
    }
    array unset ::hv3_url_tokens
}

# url_callback --
#
#         url_callback SCRIPT TOKEN
#
#     This callback is made by the http package when the response to an
#     http request has been received.
#
swproc url_callback {{type Document} token} {
  # The following line is a trick of the http package. $token is the name
  # of a Tcl array in the global context that contains the downloaded
  # information. The [upvar] command makes "state" a local alias for that
  # array variable.
  upvar #0 $token state
  
  unset ::hv3_url_tokens($token)
  urlFinish $type
  gui_log "DOWNLOAD Finished: \"$state(url)\""
  cache_store $state(url) $state(body)

  # Check if this is a redirect. If so, do not invoke $script, just fetch
  # the URL we are redirected to. TODO: Need to the -id and -cache options
  # to [url_fetch] here.
  foreach {n v} $state(meta) {
    if {[regexp -nocase ^location$ $n]} {
      foreach script $::url_g_scripts($state(url)) {
        url_fetch [string trim $v] -script $script -type $type
      }
      url_fetch [string trim $v] -type $type
      return
    }
  }
  
  foreach script $::url_g_scripts($state(url)) {
    lappend script $state(body)
    eval $script
  }

  # Cleanup the record of the download.
  ::http::cleanup $token
}
