#pragma once

/** FIXME: should be set in CMakeLists.txt */

#if defined(_WIN32)
	#define arc_PLATFORM_IS_WINDOWS 1
#else
	#define arc_PLATFORM_IS_WINDOWS 0
#endif

#if defined(__linux__)
	#define arc_PLATFORM_IS_LINUX 1
#else
	#define arc_PLATFORM_IS_LINUX 0
#endif

#if defined(__EMSCRIPTEN__)
	#define arc_PLATFORM_IS_EMSCRIPTEN 1
#else
	#define arc_PLATFORM_IS_EMSCRIPTEN 0
#endif

#if (arc_PLATFORM_IS_WINDOWS + arc_PLATFORM_IS_LINUX + arc_PLATFORM_IS_EMSCRIPTEN) != 1
	#error "There is some confusion about which platform we are on."
#endif

#if defined(__clang__)
	#define arc_COMPILER_IS_CLANG 1
#else
	#define arc_COMPILER_IS_CLANG 0
#endif

#if defined(__GNUC__) && !defined(__llvm__)
	#define arc_COMPILER_IS_GCC 1
#else
	#define arc_COMPILER_IS_GCC 0
#endif

#if defined(_MSC_VER)
	#define arc_COMPILER_IS_MSVC 1
#else
	#define arc_COMPILER_IS_MSVC 0
#endif

#if (arc_COMPILER_IS_CLANG + arc_COMPILER_IS_GCC + arc_COMPILER_IS_MSVC) != 1
	#error "There is some confusion about which compiler we are using."
#endif
