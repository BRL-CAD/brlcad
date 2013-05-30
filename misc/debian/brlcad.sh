BRLCAD_PATH=/usr/brlcad/bin
BRLCAD_MANPATH=/usr/brlcad/share/man

if [ -z "$PATH" ]
then
	export PATH=${BRLCAD_PATH}
# check if $BRLCAD_PATH is already in $PATH
elif echo $PATH | awk -F: -v j=$BRLCAD_PATH '{for(i=1;i<=NF;i++){if($i == j){exit 1}}}'
then
	export PATH=${PATH}:${BRLCAD_PATH}
fi

# check if $BRLCAD_MANPATH is already in $MANPATH
if echo $MANPATH | awk -F: -v j=$BRLCAD_MANPATH '{for(i=1;i<=NF;i++){if($i == j){exit 1}}}'
then
	# check if $MANPATH ends with a colon
	if [ "${MANPATH#${MANPATH%?}}" == ":" ]
	then
		export MANPATH=${MANPATH}${BRLCAD_MANPATH}:
	else
		export MANPATH=${MANPATH}:${BRLCAD_MANPATH}
	fi
fi
