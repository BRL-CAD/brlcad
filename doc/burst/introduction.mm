A joint-services sponsored program is currently establishing a Ballistic
Research Laboratory-Computer Aided Design (\*(B))
modeling capability at Eglin Air Force Base, Florida.
Currently, the Air Force uses \*(F) to model target geometries using
polygonal facets, two-dimensional plates, and some solid primitives.
They also maintain compatibility with \*(S), a precursor to \*(F).
These modelers are cumbersome to use by today's standards and the resultant
descriptions,
while taking an extremely long time to build,
fall far short of the accuracy and information content of true
combinatorial solid geometry models such as produced by
the \fBM\fRultiple-device \fBG\fRraphics \fBED\fRitor (\*(m)),
the \*(B) modeler.
Along with this capability comes several key requirements,
one of which will be addressed in this paper;
an interface to Eglin AFB's Point Burst Damage Assessment Model (\*(P))
from \*(B) solid models.
.P
The \*(b) program enables the user to direct ray tracing
of the \*(m) models to prepare inputs to the
\*(P) code in the form of shotline files and burst point library files.
The program is capable of supporting analyses of varied threat
mechanisms on \*(m) targets.
The shotline and burst point library files are written in
.SM ASCII
and are supplemented with both shaded, color images
and 3-d vector plots using the \*(B) frame buffer (\*(f)) and extended
\*(U)
.FS \*(Tm
UNIX is a registered trademark of AT&T.
.FE
plot (\*(p)) libraries for portability to virtually all hardware
platforms running the \*(U) operating system.
The terminal-independent user interface employs hierarchical menus
coupled with optional graphics displays facilitating ease of use and
maximizing information feed back to the operator while permitting full
featured use from any video terminal or window system.
The menu-driven interface is coupled with an interpreter that
accepts key word commands for batch input if the user wishes to
detach the job to run unattended.
These key word command files can either be prepared automatically from
the user-selected parameters orchestrated by the menu system,
typed in by hand with an editor,
or perhaps generated from another process in a pipeline configuration.
.P
This paper is intended to serve a dual function,
both as a technical memorandum,
and as a user manual.
Hopefully,
not only will it help one appreciate why \*(b) was developed in the first place
but will help readers decide whether they need to acquire it,
and if so,
how to use it.
