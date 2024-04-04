#
# tkhtml.tcl --
#
#     This file contains:
#
#         - The default bindings for the Html widget, and
#         - Some Tcl functions used by the stylesheet html.css.
#
# ------------------------------------------------------------------------
#
# Copyright (c) 2005 Eolas Technologies Inc.
# All rights reserved.
# 
# This Open Source project was made possible through the financial support
# of Eolas Technologies Inc.
# 
# Redistribution and use in source and binary forms, with or without
# modification, are permitted provided that the following conditions are met:
# 
#     * Redistributions of source code must retain the above copyright
#       notice, this list of conditions and the following disclaimer.
#     * Redistributions in binary form must reproduce the above copyright
#       notice, this list of conditions and the following disclaimer in the
#       documentation and/or other materials provided with the distribution.
#     * Neither the name of the <ORGANIZATION> nor the names of its
#       contributors may be used to endorse or promote products derived from
#       this software without specific prior written permission.
# 
# THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS "AS IS"
# AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE
# IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE
# ARE DISCLAIMED. IN NO EVENT SHALL THE COPYRIGHT OWNER OR CONTRIBUTORS BE
# LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR
# CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF
# SUBSTITUTE GOODS OR SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS
# INTERRUPTION) HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN
# CONTRACT, STRICT LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE)
# ARISING IN ANY WAY OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF THE
# POSSIBILITY OF SUCH DAMAGE.
#

switch -- $::tcl_platform(platform) {
  windows {
    bind Html <MouseWheel>   { %W yview scroll [expr %D/-30] units }
  }
  macintosh {
    bind Html <MouseWheel>   { %W yview scroll [expr %D*-4] units }
  }
  default {
    # Assume X windows by default.
    bind Html <ButtonPress-4>   { %W yview scroll -4 units }
    bind Html <ButtonPress-5>   { %W yview scroll  4 units }
  }
}


# Some Tcl procs used by html.css
#
namespace eval tkhtml {

    # This is called for <input type=text> tags that have a size
    # attribute. The size attribute in this case is supposed to be
    # the width in characters.
    proc inputsize_to_css {} {
        upvar N node
        set size [$node attr size]
        catch {
          if {$size < 0} {error "Bad value for size attribute"}
        }

        # Figure out if we are talking characters or pixels:
        switch -- [string tolower [$node attr -default text type]] {
          text     { 
            incr size [expr {int(($size/10)+1)}]
            set units ex 
          }
          password { 
            incr size [expr {int(($size/10)+1)}]
            set units ex 
          }
          file     { 
            incr size 10 
            set units ex 
          }
          default  { set units px }
        }

        return "${size}${units}"
    }

    proc if_disabled {if else} {
      upvar N node
      set disabled [$node attr -default 0 disabled]
      if {$disabled} {return $if}
      return $else
    }
    
    # The following two procs are used to determine the width and height of
    # <textarea> markups. Technically speaking, the "cols" and "rows"
    # attributes are compulsory for <textarea> elements.
    proc textarea_width {} {
        upvar N node
        set cols [$node attr -default "" cols]
        if {[regexp {[[:digit:]]+}] $cols} { return "${cols}ex" }
        return $cols
    }
    proc textarea_height {} {
        upvar N node
        set rows [$node attr -default "" rows]
        if {[regexp {[[:digit:]]+} $rows]} { return "[expr ${rows} * 1.2]em" }
        return $rows
    }

    proc size_to_fontsize {} {
        upvar N node
        set size [$node attr size]

        if {![regexp {([+-]?)([0123456789]+)} $size dummy sign quantity]} {
          error "not an integer"
        }

        if {$sign eq ""} {
            switch -- $quantity {
                1 {return xx-small}
                2 {return small}
                3 {return medium}
                4 {return large}
                5 {return x-large}
                6 {return xx-large}
                default { error "out of range: $size" }
            }
        }

        if {$sign eq "-"} {
            if {$quantity eq "1"} {return smaller}
            return "[expr 100 * pow(0.85, $quantity)]%"
        }

        if {$sign eq "+"} {
            if {$quantity eq "1"} {return larger}
            return "[expr 100 * pow(1.176, $quantity)]%"
        }

        error "logic error"
    }

    proc vscrollbar {base node} {
      set sb [scrollbar ${base}.vsb_[string map {: _} $node]]
      $sb configure -borderwidth 1 -highlightthickness 0 -command "$node yview"
      return $sb
    }
    proc hscrollbar {base node} {
      set sb [scrollbar ${base}.hsb_[string map {: _} $node] -orient horiz]
      $sb configure -borderwidth 1 -highlightthickness 0 -command "$node xview"
      return $sb
    }

    proc ol_liststyletype {} {
      switch -exact -- [uplevel {$N attr type}] {
        i {return lower-roman}
        I {return upper-roman}
        a {return lower-alpha}
        A {return upper-alpha}
        1 {return decimal}
      }
      error "Unrecognized type attribute on OL element"
    }
}

