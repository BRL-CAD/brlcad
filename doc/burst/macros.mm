'\" define macro for program usage examples
.de Ex
.sp
.in +0.5i
\fI$\ \\$1\fR
.in -0.5i
.sp
..
'\" define macro for naming coordinate axes
.de Ax
\\$1, \\$2, and \\$3\fI axes\fR
..
'\" define macro for naming commands
.de Co
\fB\\$1\fR
..
'\" define macros for command descriptions.
.de Cs                  \" Start command description.
.br
\s-1\fB[menu reference:\\$1]\fR\s0
.P
..
.de Ce                  \" End command description.
.sp
..
.de Ud
.BI \\$1 (\\$2)\^
..
