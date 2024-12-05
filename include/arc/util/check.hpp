#pragma once

#include "arc/util/tracing.hpp"

#include <exception>

/**
 * Always terminate if the expression evaluates to false.
 */
#define arc_CHECK_Require(expression)                                                              \
	do                                                                                             \
	{                                                                                              \
		if (!static_cast<bool>(expression))                                                        \
		{                                                                                          \
			arc_TRACE_MESSAGE(true, "assert failed");                                              \
			std::terminate();                                                                      \
		}                                                                                          \
	} while (0)

#if 1
	#define arc_CHECK_Assert(expression) arc_CHECK_Require(expression)
	#define arc_CHECK_Precondition(expression) arc_CHECK_Require(expression)
#else
	#define arc_CHECK_Assert(expression) [[assume(!!(expression))]]
	#define arc_CHECK_Precondition(expression) [[assume(!!(expression))]]
#endif
