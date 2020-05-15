

# save the precious args
ARGS="$@"
NAME_OF_THIS="`basename \"$0\"`"
PATH_TO_THIS="`dirname \"$0\"`"
THIS="$PATH_TO_THIS/$NAME_OF_THIS"

${PATH_TO_THIS}/fuzz_test


echo ${PATH_TO_THIS}
