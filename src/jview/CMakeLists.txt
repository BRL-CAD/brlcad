# install openjdk-17-jdk (NOT the headless package, we need awt
# for these demos)

# To build and run JOGLQuad in isolation, the following works:
# javac -classpath "/usr/share/java/gluegen2-rt.jar:/usr/share/java/jogl2-2.3.2.jar" JOGLQuad.java
# java -classpath "/usr/share/java/gluegen2-rt.jar:/usr/share/java/jogl2-2.3.2.jar:." JOGLQuad
#
# To run the ones built by CMake (there may be other ways, these are just the
# ones that worked first here:)
#
# java -classpath "src/jview/JOGLQuad.jar:/usr/share/java/gluegen2-rt.jar:/usr/share/java/jogl2-2.3.2.jar:." JOGLQuad
#
# mkdir -p demos/es2
# cp src/jview/RawGL2ES2demo.jar demos/es2/
# java -cp "demos/es2/RawGL2ES2demo.jar:/usr/share/java/gluegen2-rt.jar:/usr/share/java/jogl2-2.3.2.jar:." demos.es2.RawGL2ES2demo

find_package(Java)
include(UseJava)
find_package(JOGL)
if (Java_JAVAC_EXECUTABLE AND JOGL_LIB AND JOGL_GLUE)
  add_jar(JOGLQuad JOGLQuad.java
    INCLUDE_JARS ${JOGL_LIB} ${JOGL_GLUE}
    )
  add_jar(RawGL2ES2demo RawGL2ES2demo.java
    INCLUDE_JARS ${JOGL_LIB} ${JOGL_GLUE}
    )
endif (Java_JAVAC_EXECUTABLE AND JOGL_LIB AND JOGL_GLUE)


# Local Variables:
# tab-width: 8
# mode: cmake
# indent-tabs-mode: t
# End:
# ex: shiftwidth=2 tabstop=8
