#pragma once

#include "arc/util/platform.hpp"

#include <chrono>
#include <exception>
#include <filesystem>
#include <memory>
#include <string>
#include <thread>
#include <utility>

#if arc_PLATFORM_IS_WINDOWS
	#define arc_DEBUG_Break(condition)                                                             \
		do                                                                                         \
		{                                                                                          \
			if (condition)                                                                         \
				__debugbreak();                                                                    \
		} while (0)
#else
	/** not implemented */
	#define arc_DEBUG_Break(condition) (void)0
#endif

struct arc_DEBUG_NonCopyable
{
	arc_DEBUG_NonCopyable() = default;

	explicit arc_DEBUG_NonCopyable(std::string_view string)
		: string{ string }
	{}

	arc_DEBUG_NonCopyable(int number, std::string_view string)
		: number{ number }
		, string{ string }
	{}

	arc_DEBUG_NonCopyable(const arc_DEBUG_NonCopyable & other) = delete;

	arc_DEBUG_NonCopyable & operator=(const arc_DEBUG_NonCopyable & other) = delete;

	arc_DEBUG_NonCopyable(arc_DEBUG_NonCopyable && other)
		: number{ std::exchange(other.number, 0) }
		, string{ std::exchange(other.string, "") }
	{}

	arc_DEBUG_NonCopyable & operator=(arc_DEBUG_NonCopyable && other)
	{
		if (this != std::addressof(other))
		{
			number = std::exchange(other.number, 0);
			string = std::exchange(other.string, "");
		}
		return *this;
	}

	int number = 0;
	std::string string;
};

inline void arc_DEBUG_SleepForever()
{
	while (true)
		std::this_thread::sleep_for(std::chrono::hours{ 1 });
}

inline bool arc_DEBUG_StopRequested()
{
#if 1
	std::error_code ec;
	bool exists = std::filesystem::exists(".Stop", ec);
	return ec ? false : exists;
#else
	return false;
#endif
}

inline void arc_DEBUG_Inspect(const std::exception_ptr & exception)
{
#if 0
	try
	{
		std::rethrow_exception(exception);
	}
	catch (const std::exception & e)
	{
		arc_DEBUG_Break(true);
	}
	catch (...)
	{
		arc_DEBUG_Break(true);
	}
#endif
}
