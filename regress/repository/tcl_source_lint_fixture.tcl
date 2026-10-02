#                    T C L _ S O U R C E _ L I N T _ F I X T U R E . T C L
# BRL-CAD
#
# Copyright (c) 2026 United States Government as represented by
# the U.S. Army Research Laboratory.
#
# Redistribution and use in source and binary forms, with or without
# modification, are permitted provided that the following conditions
# are met:
#   1. Redistributions of source code must retain the above copyright
#      notice, this list of conditions and the following disclaimer.
#   2. Redistributions in binary form must reproduce the above copyright
#      notice, this list of conditions and the following disclaimer in the
#      documentation and/or other materials provided with the distribution.
#   3. The name of the author may not be used to endorse or promote
#      products derived from this software without specific prior written
#      permission.
#
# THIS SOFTWARE IS PROVIDED BY THE AUTHOR ``AS IS'' AND ANY EXPRESS OR
# IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE IMPLIED WARRANTIES
# OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE ARE DISCLAIMED.
# IN NO EVENT SHALL THE AUTHOR BE LIABLE FOR ANY DIRECT, INDIRECT,
# INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT
# NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE,
# DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY
# THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
# (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE OF
# THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
#
###
#
# Tcl source lint parser fixture.  The marker comments are the expected
# diagnostics consumed by tcl_source_lint --self-test.

proc ::legacy_cmd {args} {
    tailcall ::brlcad::test command {*}$args
}

legacy_cmd top_level                         ; # lint ::legacy_cmd

proc ::native_shadow {args} {
    return $args
}
native_shadow same_source

runtime_shadow application_command
::runtime_shadow explicit                    ; # lint ::runtime_shadow

proc ::fixture::control_flow {value} {
    if {[legacy_cmd condition]} {            ; # lint ::legacy_cmd
	_mged_draw all.g                       ; # lint ::_mged_draw
    }
    foreach item {a b} {
	::legacy_cmd $item                     ; # lint ::legacy_cmd
    }
    switch -- $value {
	a {legacy_cmd switch_body; # lint ::legacy_cmd
	}
	b -
	default {set value clean}
    }
    try {
	legacy_cmd try_body                     ; # lint ::legacy_cmd
    } finally {
	set value done
    }
}

namespace eval ::fixture::callbacks {
    proc install {} {
	after idle [list legacy_cmd callback]   ; # lint ::legacy_cmd
	bind .fixture <Expose> {legacy_cmd bind}; # lint ::legacy_cmd
	bind .fixture <MouseWheel> {expr {-%D/120}}
	eval {legacy_cmd eval_body}             ; # lint ::legacy_cmd
	tailcall legacy_cmd tail                ; # lint ::legacy_cmd
    }
}

namespace eval ::fixture::shadowed {
    proc legacy_cmd {args} {
	return $args
    }
    proc allowed {} {
	legacy_cmd local
	::legacy_cmd explicit                   ; # lint ::legacy_cmd
	rename legacy_cmd saved
	info commands legacy_cmd
	set command legacy_cmd
	$command dynamic
    }
}

::itcl::class FixtureWidget {
    public {
	method legacy_cmd {args}
	method local_call {} {legacy_cmd local}
	method explicit_call {} {
	    ::legacy_cmd explicit               ; # lint ::legacy_cmd
	}
    }
}

::itcl::class ExternalFixtureWidget {
    inherit ::external::Widget

    method inherited_call {} {
	legacy_cmd inherited
	::legacy_cmd explicit                   ; # lint ::legacy_cmd
    }
}

::itcl::class DynamicFixtureWidget {
    inherit $::fixture_base

    method inherited_call {} {
	legacy_cmd inherited
	::legacy_cmd explicit                   ; # lint ::legacy_cmd
    }
}

::itcl::body OtherWidget::external_body {} {
    legacy_cmd unshadowed                      ; # lint ::legacy_cmd
}
