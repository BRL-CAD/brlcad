BRLCAD_PATH=/usr/brlcad/bin
BRLCAD_MANPATH=/usr/brlcad/share/man

if [ -z "$PATH" ]
then
	export PATH=${BRLCAD_PATH}
else
	export PATH=${PATH}:${BRLCAD_PATH}
fi

if [ -z "$MANPATH" ]
then
	export MANPATH=${BRLCAD_MANPATH}
else
	export MANPATH=${MANPATH}:${BRLCAD_MANPATH}
fi
