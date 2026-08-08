#pragma once

#include "arc/util/tracing.hpp"

#include <cstdio>
#include <exception>
#include <format>
#include <print>
#include <string>

#if arc_PLATFORM_IS_WINDOWS && arc_CHECK_SILENT_ABORT
	#include <cstdlib>
#endif

#if __cpp_lib_stacktrace >= 202011L
	#include <stacktrace>
#else
	#include <source_location>
#endif

namespace arc::detail
{
	/**
	 * Reports a failed check on stderr, naming the expression and its location.
	 * stdout is flushed first because std::terminate() may discard buffered data.
	 */
	[[noreturn]] inline void ReportCheckFailure(
		const char * expression,
#if __cpp_lib_stacktrace >= 202011L
		const std::stacktrace & s = std::stacktrace::current()
#else
		const std::source_location & s = std::source_location::current()
#endif
	)
	{
#if __cpp_lib_stacktrace >= 202011L
		std::string location;
		for (const auto & frame : s)
			location += "| " + std::to_string(frame) + "\n";
#else
		std::string location = std::format(
			"| {}:{}\n"
			"| Stack trace not supported by compiler.\n",
			s.file_name(), s.line());
#endif

		std::fflush(stdout);
		std::println(
			stderr,
			"/---- CHECK FAILED -----\n"
			"| Expression: static_cast<bool>({})\n"
			"{}"
			"\\-----------------------",
			expression, location);
		std::fflush(stderr);

#if arc_PLATFORM_IS_WINDOWS && arc_CHECK_SILENT_ABORT
		/**
		 * std::terminate() ends up in abort(). The report above is the whole diagnostic, so the
		 * process exits silently instead of raising the CRT abort message box and the Windows
		 * Error Reporting dialog.
		 */
		_set_abort_behavior(0, _WRITE_ABORT_MSG | _CALL_REPORTFAULT);
#endif

		std::terminate();
	}
}

/**
 * Always terminate if the expression evaluates to false.
 */
#define arc_CHECK_Require(expression)                                                              \
	do                                                                                             \
	{                                                                                              \
		if (!static_cast<bool>(expression))                                                        \
		{                                                                                          \
			arc_TRACE_MESSAGE(true, "assert failed");                                              \
			arc::detail::ReportCheckFailure(#expression);                                          \
		}                                                                                          \
	} while (0)

#if 1
	#define arc_CHECK_Assert(expression) arc_CHECK_Require(expression)
	#define arc_CHECK_Precondition(expression) arc_CHECK_Require(expression)
#else
	#define arc_CHECK_Assert(expression) [[assume(!!(expression))]]
	#define arc_CHECK_Precondition(expression) [[assume(!!(expression))]]
#endif
