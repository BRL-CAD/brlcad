The directories embedded and other contain the licenses for components that end
up distributed in the BRL-CAD binary packages.  For 3rd party components that
are to be included in a built BRL-CAD binary package in some form (as opposed
to being simply part of the build infrastructure) we keep copies of the
licenses here so they may be bundled and distributed with the built binaries.

The "embedded" directory holds licenses for code that is incorporated in one of
BRL-CAD's libraries, main repository directories, or sometimes directly in
source files.

Embedded license files use "file:/path" to identify repository files containing
the licensed material.  For external header-only dependencies, "usedby:/path"
identifies repository files that consume the dependency without implying that
the consumers contain the upstream source.  Both forms require repository-root
relative paths to existing files.

Historically we stored license for codes that are included with the binaries
but not directly incorporated into our own code here in an "other" directory,
but now these licenses are provided from the bext dependency repository as
part of its output.

