# CMAKE generated file: DO NOT EDIT!
# Generated by "Unix Makefiles" Generator, CMake Version 3.22

# Delete rule output on recipe failure.
.DELETE_ON_ERROR:

#=============================================================================
# Special targets provided by cmake.

# Disable implicit rules so canonical targets will work.
.SUFFIXES:

# Disable VCS-based implicit rules.
% : %,v

# Disable VCS-based implicit rules.
% : RCS/%

# Disable VCS-based implicit rules.
% : RCS/%,v

# Disable VCS-based implicit rules.
% : SCCS/s.%

# Disable VCS-based implicit rules.
% : s.%

.SUFFIXES: .hpux_make_needs_suffix_list

# Command-line flag to silence nested $(MAKE).
$(VERBOSE)MAKESILENT = -s

#Suppress display of executed commands.
$(VERBOSE).SILENT:

# A target that is always out of date.
cmake_force:
.PHONY : cmake_force

#=============================================================================
# Set environment variables for the build.

# The shell in which to execute make rules.
SHELL = /bin/sh

# The CMake executable.
CMAKE_COMMAND = /usr/bin/cmake

# The command to remove a file.
RM = /usr/bin/cmake -E rm -f

# Escaping for special characters.
EQUALS = =

# The top-level source directory on which CMake was run.
CMAKE_SOURCE_DIR = /home/xuanxuan/coding/tmp/tmp.YvusP4fwtS

# The top-level build directory on which CMake was run.
CMAKE_BINARY_DIR = /home/xuanxuan/coding/tmp/tmp.YvusP4fwtS/cmake-build-debug

# Include any dependencies generated for this target.
include CMakeFiles/Web_Server.dir/depend.make
# Include any dependencies generated by the compiler for this target.
include CMakeFiles/Web_Server.dir/compiler_depend.make

# Include the progress variables for this target.
include CMakeFiles/Web_Server.dir/progress.make

# Include the compile flags for this target's objects.
include CMakeFiles/Web_Server.dir/flags.make

CMakeFiles/Web_Server.dir/main.cpp.o: CMakeFiles/Web_Server.dir/flags.make
CMakeFiles/Web_Server.dir/main.cpp.o: ../main.cpp
CMakeFiles/Web_Server.dir/main.cpp.o: CMakeFiles/Web_Server.dir/compiler_depend.ts
	@$(CMAKE_COMMAND) -E cmake_echo_color --switch=$(COLOR) --green --progress-dir=/home/xuanxuan/coding/tmp/tmp.YvusP4fwtS/cmake-build-debug/CMakeFiles --progress-num=$(CMAKE_PROGRESS_1) "Building CXX object CMakeFiles/Web_Server.dir/main.cpp.o"
	/usr/bin/c++ $(CXX_DEFINES) $(CXX_INCLUDES) $(CXX_FLAGS) -MD -MT CMakeFiles/Web_Server.dir/main.cpp.o -MF CMakeFiles/Web_Server.dir/main.cpp.o.d -o CMakeFiles/Web_Server.dir/main.cpp.o -c /home/xuanxuan/coding/tmp/tmp.YvusP4fwtS/main.cpp

CMakeFiles/Web_Server.dir/main.cpp.i: cmake_force
	@$(CMAKE_COMMAND) -E cmake_echo_color --switch=$(COLOR) --green "Preprocessing CXX source to CMakeFiles/Web_Server.dir/main.cpp.i"
	/usr/bin/c++ $(CXX_DEFINES) $(CXX_INCLUDES) $(CXX_FLAGS) -E /home/xuanxuan/coding/tmp/tmp.YvusP4fwtS/main.cpp > CMakeFiles/Web_Server.dir/main.cpp.i

CMakeFiles/Web_Server.dir/main.cpp.s: cmake_force
	@$(CMAKE_COMMAND) -E cmake_echo_color --switch=$(COLOR) --green "Compiling CXX source to assembly CMakeFiles/Web_Server.dir/main.cpp.s"
	/usr/bin/c++ $(CXX_DEFINES) $(CXX_INCLUDES) $(CXX_FLAGS) -S /home/xuanxuan/coding/tmp/tmp.YvusP4fwtS/main.cpp -o CMakeFiles/Web_Server.dir/main.cpp.s

# Object files for target Web_Server
Web_Server_OBJECTS = \
"CMakeFiles/Web_Server.dir/main.cpp.o"

# External object files for target Web_Server
Web_Server_EXTERNAL_OBJECTS =

Web_Server: CMakeFiles/Web_Server.dir/main.cpp.o
Web_Server: CMakeFiles/Web_Server.dir/build.make
Web_Server: CMakeFiles/Web_Server.dir/link.txt
	@$(CMAKE_COMMAND) -E cmake_echo_color --switch=$(COLOR) --green --bold --progress-dir=/home/xuanxuan/coding/tmp/tmp.YvusP4fwtS/cmake-build-debug/CMakeFiles --progress-num=$(CMAKE_PROGRESS_2) "Linking CXX executable Web_Server"
	$(CMAKE_COMMAND) -E cmake_link_script CMakeFiles/Web_Server.dir/link.txt --verbose=$(VERBOSE)

# Rule to build all files generated by this target.
CMakeFiles/Web_Server.dir/build: Web_Server
.PHONY : CMakeFiles/Web_Server.dir/build

CMakeFiles/Web_Server.dir/clean:
	$(CMAKE_COMMAND) -P CMakeFiles/Web_Server.dir/cmake_clean.cmake
.PHONY : CMakeFiles/Web_Server.dir/clean

CMakeFiles/Web_Server.dir/depend:
	cd /home/xuanxuan/coding/tmp/tmp.YvusP4fwtS/cmake-build-debug && $(CMAKE_COMMAND) -E cmake_depends "Unix Makefiles" /home/xuanxuan/coding/tmp/tmp.YvusP4fwtS /home/xuanxuan/coding/tmp/tmp.YvusP4fwtS /home/xuanxuan/coding/tmp/tmp.YvusP4fwtS/cmake-build-debug /home/xuanxuan/coding/tmp/tmp.YvusP4fwtS/cmake-build-debug /home/xuanxuan/coding/tmp/tmp.YvusP4fwtS/cmake-build-debug/CMakeFiles/Web_Server.dir/DependInfo.cmake --color=$(COLOR)
.PHONY : CMakeFiles/Web_Server.dir/depend
