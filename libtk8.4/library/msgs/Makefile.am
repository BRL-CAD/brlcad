# $Id$


FILES = cs.msg de.msg el.msg en_gb.msg en.msg es.msg fr.msg it.msg nl.msg ru.msg

install:
	mkdir -p @TK_LIBRARY@/msgs
	for a in $(FILES) ; do ${INSTALL_SCRIPT} -m 444 $$a @TK_LIBRARY@/msgs/$$a ; done
