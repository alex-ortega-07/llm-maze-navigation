# ==============================================
#  Makefile - Maze generator (SDL2)
#  Explicit version: one rule per file
# ==============================================
#
#  Usage (in PowerShell use "mingw32-make"):
#    make         -> build the program
#    make run     -> build and run
#    make clean   -> remove generated files
#

CXX      = g++
CXXFLAGS = -std=c++17 -Wall -Wextra
LIBS     = -lSDL2 -lSDL2_ttf


# --- Shell commands (they depend on where make is run) ---
# mingw32-make (PowerShell / cmd) has no "rm" or "mkdir -p",
# so it uses Windows cmd commands instead.
ifeq ($(MAKE_HOST),Windows32)
    SHELL       = cmd.exe
    MKDIR_BUILD = if not exist build mkdir build
    REMOVE_ALL  = if exist build rmdir /s /q build & if exist maze.exe del /q maze.exe
    RUN_PROGRAM = .\maze.exe
else
    MKDIR_BUILD = mkdir -p build
    REMOVE_ALL  = rm -rf build maze.exe
    RUN_PROGRAM = ./maze.exe
endif


# --- Default target ---
all: maze.exe


# --- Final step: link the .o files into the executable ---
maze.exe: build/main.o build/Cell.o build/StdinReader.o
	$(CXX) build/main.o build/Cell.o build/StdinReader.o -o maze.exe $(LIBS)


# --- Compile main.cpp ---
# Rebuilt if main.cpp or any header it includes changes
build/main.o: src/main.cpp src/Cell.hpp src/StdinReader.hpp
	$(MKDIR_BUILD)
	$(CXX) $(CXXFLAGS) -c src/main.cpp -o build/main.o


# --- Compile Cell.cpp ---
build/Cell.o: src/Cell.cpp src/Cell.hpp
	$(MKDIR_BUILD)
	$(CXX) $(CXXFLAGS) -c src/Cell.cpp -o build/Cell.o


# --- Compile StdinReader.cpp ---
build/StdinReader.o: src/StdinReader.cpp src/StdinReader.hpp
	$(MKDIR_BUILD)
	$(CXX) $(CXXFLAGS) -c src/StdinReader.cpp -o build/StdinReader.o


# --- Shortcuts ---
run: maze.exe
	$(RUN_PROGRAM)

clean:
	$(REMOVE_ALL)

.PHONY: all run clean
