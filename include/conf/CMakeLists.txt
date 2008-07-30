# update COUNT
find_file(countFile COUNT ${CMAKE_CURRENT_SOURCE_DIR})

if(countFile)
    file(READ COUNT buildCounter)
else(countFile)
    set(buildCounter 0)
endif(countFile)

math(EXPR buildCounter "${buildCounter} + 1")
file(WRITE COUNT ${buildCounter})


# update DATE
execute_process(COMMAND cmd /c date /t OUTPUT_VARIABLE currentDate OUTPUT_STRIP_TRAILING_WHITESPACE)
execute_process(COMMAND cmd /c time /t OUTPUT_VARIABLE currentTime OUTPUT_STRIP_TRAILING_WHITESPACE)

file(WRITE DATE "\"${currentDate} ${currentTime}\"")


# update HOST
file(WRITE HOST "\"$ENV{COMPUTERNAME}\"")


# update PATH
set(programPath "\"$ENV{ProgramFiles}\\BRL-CAD\"")
string(REPLACE "\\" "\\\\" programPathForC ${programPath})
file(WRITE PATH ${programPathForC})


# update USER
file(WRITE USER "\"$ENV{USERNAME}\"")
