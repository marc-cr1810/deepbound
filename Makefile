.PHONY: all debug release clean

all: debug

debug:
	cmake -E make_directory build/debug
	cmake -S . -B build/debug -DCMAKE_BUILD_TYPE=Debug
	cmake --build build/debug

release:
	cmake -E make_directory build/release
	cmake -S . -B build/release -DCMAKE_BUILD_TYPE=Release
	cmake --build build/release

clean:
	cmake -E remove_directory build
