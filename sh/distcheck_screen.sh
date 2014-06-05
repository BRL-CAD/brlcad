#!/bin/sh
#             D I S T C H E C K  _ S C R E E N . S H
# BRL-CAD
#
# Copyright (c) 2012-2014 United States Government as represented by
# the U.S. Army Research Laboratory.
#
# Redistribution and use in source and binary forms, with or without
# modification, are permitted provided that the following conditions
# are met:
#
# 1. Redistributions of source code must retain the above copyright
# notice, this list of conditions and the following disclaimer.
#
# 2. Redistributions in binary form must reproduce the above
# copyright notice, this list of conditions and the following
# disclaimer in the documentation and/or other materials provided
# with the distribution.
#
# 3. The name of the author may not be used to endorse or promote
# products derived from this software without specific prior written
# permission.
#
# THIS SOFTWARE IS PROVIDED BY THE AUTHOR ``AS IS'' AND ANY EXPRESS
# OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE IMPLIED
# WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE
# ARE DISCLAIMED. IN NO EVENT SHALL THE AUTHOR BE LIABLE FOR ANY
# DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL
# DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE
# GOODS OR SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS
# INTERRUPTION) HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY,
# WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT (INCLUDING
# NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE OF THIS
# SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
#
###
#
# This is a simple listing of screen commands that (when run from
# a session of GNU screen 4.1 or greater) will set up a convenient
# set of screen windows within which to run and monitor the distcheck
# process.  To use it, do something like the following from your
# toplevel build directory:
#
# screen
# sh distcheck_screen.sh
# make -j9 distcheck
#
# (tweak the configure line as appropriate)
#
# If you want more windows for more than just the standard distcheck,
# uncomment the "split/focus/select 3" lines + the tail command pertaining
# to -p 3 below and add as many more similar lines as are needed.  Other
# window geometries are possible - this is just intended as a convenience
# for running the common pre-defined cases.
#
# To exit from the whole thing, halt the build (if necessary) and type
# screen -X quit in the same window the toplevel distcheck make is being run.
###


screen -X screen
screen -X screen
screen -X screen


screen -X -p 0 title "Main"
screen -X select 0
screen -X split -v
screen -X focus
screen -X select 1
screen -X -p 1 title "Debug"
screen -X split
screen -X focus
screen -X select 2
screen -X -p 2 title "Release"
#screen -X split
#screen -X focus
#screen -X select 3
#screen -X -p 3 title "Tk Disabled"
screen -X focus

screen -X -p 1 exec tail --retry --follow=name distcheck-debug.log
screen -X -p 2 exec tail --retry --follow=name distcheck-release.log
#screen -X -p 3 exec tail --retry --follow=name distcheck-no_tk.log

screen -X -p 0 exec cmake ../brlcad -DBRLCAD_BUNDLED_LIBS=ON
