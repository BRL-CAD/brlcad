Remaining items:

1.  Detect OpenGL properly on Apple - choose X11 vs Aqua, and get the ogl code working

2.  Do a diff of all the generated scripts using configure_file - make sure what autotools produces 
	 is what is being produced by CMake, and make sure no variable definitions in support of scripts 
	 are nuking variables used by CMake

3.  Our tcl autopath function for adding paths to the package search list needs fixing - CMake breaks
    assumptions it was using.

4.  Scrub the third party logic and clean up/simplify - try to get away from using BRLCAD_ variables
    when they aren't needed.

5.  Break logic out of the toplevel into src and src/other dirs - among other things, we want to
    be able to cd in to src and type make to avoid the doc subdirectory.

6.  Review the dist logic in the toplevel Makefile.am.  Gonna have to study up on CPack and CTest -
	 figure out the process for checking permissions, install results, etc. in order to provide
	 the same robustness for CMake generated tarballs that we have for autotools.

7.  Build flags - we supply a lot via several options in autotools - express that in CMake

8.  Review and test binaries - get regression testing working, check mged and archer, etc.

9.  Multiplatform testing.  Specifically, find a Windows box and conditionalize everything which
    doesn't work out-of-box (lex/yacc and sh based logic are obvious, other probables)
