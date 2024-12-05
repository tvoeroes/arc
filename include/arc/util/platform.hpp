#pragma once

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

#if (arc_PLATFORM_IS_WINDOWS + arc_PLATFORM_IS_LINUX) != 1
	#error "There is some confusion about which platform we are on."
#endif
