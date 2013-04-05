BRLCAD_PATH=/usr/brlcad/bin
BRLCAD_MANPATH=/usr/brlcad/share/man

if [ -z "$PATH" ]
then
	export PATH=${BRLCAD_PATH}
# check if $BRLCAD_PATH is already in $PATH
elif awk -F: -v j=$BRLCAD_PATH '{for(i=1;i<=NF;i++){if($i == j){exit 1}}}'<<<$PATH
then
	export PATH=${PATH}:${BRLCAD_PATH}
fi

if [ -z "$MANPATH" ]
then
	export MANPATH=${BRLCAD_MANPATH}
# check if $BRLCAD_MANPATH is already in $MANPATH
elif awk -F: -v j=$BRLCAD_MANPATH '{for(i=1;i<=NF;i++){if($i == j){exit 1}}}'<<<$MANPATH
then
	export MANPATH=${MANPATH}:${BRLCAD_MANPATH}
fi
