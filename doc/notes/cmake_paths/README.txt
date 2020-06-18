This is a minimal example of combining generator expressions, configure_file
and cmake -P to execute a script at runtime with a multiconfig generator and a
non-standard BIN_DIR.

Multiconfig build tools change their "root" build output directory at runtime
to match the current configuration, which means scripts cannot assume a fixed
path to the build output files.  Historically BRL-CAD handled this by writing
specific files into the CMake temporary directories in the build dir from a
custom target in the parent build which was then depended upon by all other
targets, and having scripts check the current state of that file.  However,
with the introduction of generator expressions, another mechanism is possible -
passing an executable build target path to the executing script via a -D option
and having the script "unpack" that path to deduce the paths.  Unlike BRL-CAD's
custom solution, this uses standard CMake functionality (given a sufficiently
new CMake.)

However, to do this robustly a couple of things are needed.  One is an
executable or library target - custom targets do not provide generator
expression output.  This is easily handled by making a "dummy" executable whose
sole purpose is to provide the CMake information from an add_executable target
(the drawback is that it introduces the requirement for a C compiler, but all
projects BRL-CAD is likely to be using CMake for should have such a compiler
present - and in principle any target language will work as long as its build
target defines the TARGET_FILE* properties.)

Another wrinkle is that the notion of a "bin" directory and "root" directory
are BRL-CAD conventions, and thus not backed into the CMake target properties.
To be able to deduce a "root" path, it is necessary to pass the BIN_DIR
variable into the script to allow the script to "strip down" the generator
supplied executable path to the root path. (In this example configure_file
handles it, since the BIN_DIR subpath does not change in the build tool at
runtime - it's generally easier to read a custom target/command with a smaller
number of variables passed as -D options.)

Both single and multiconfig behaviors can be tested using the Ninja build
tool (with recent CMake and Ninja):

user@ubuntu2019:~/cmake_paths/build$ cmake -G Ninja .. && ninja dinfo
-- Configuring done
-- Generating done
-- Build files have been written to: /home/user/cmake_paths/build
[3/3] cd /home/user/cmake_paths/build && /home/user/cmake-install/... -DEXEC_DIR=/home/user/cmake_paths/build/deep/dir/bin -P test.cmake
EXEC_DIR:/home/user/cmake_paths/build/deep/dir/bin
BIN_DIR:deep/dir/bin
ROOT_DIR:/home/user/cmake_paths/build/

user@ubuntu2019:~/cmake_paths/build$ cmake -G "Ninja Multi-Config" .. && ninja -f build-Debug.ninja dinfo && ninja -f build-Release.ninja dinfo
-- Configuring done
-- Generating done
-- Build files have been written to: /home/user/cmake_paths/build
[1/1] cd /home/user/cmake_paths/build && /home/user/cmake-install/...C_DIR=/home/user/cmake_paths/build/Debug/deep/dir/bin -P test.cmake
EXEC_DIR:/home/user/cmake_paths/build/Debug/deep/dir/bin
BIN_DIR:deep/dir/bin
ROOT_DIR:/home/user/cmake_paths/build/Debug/
[1/1] cd /home/user/cmake_paths/build && /home/user/cmake-install/...DIR=/home/user/cmake_paths/build/Release/deep/dir/bin -P test.cmake
EXEC_DIR:/home/user/cmake_paths/build/Release/deep/dir/bin
BIN_DIR:deep/dir/bin
ROOT_DIR:/home/user/cmake_paths/build/Release/

