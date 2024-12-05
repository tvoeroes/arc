#pragma once

#include "arc/fwd.hpp"
#include "arc/util/std.hpp"

struct arc::options
{
	size_t workerThreadCount = 0;
	std::thread::id mainThreadId;
	std::span<const char * const> args;

	static options hardware_concurrency();

	static options hardware_concurrency_no_main_thread();

	static options two_threads();

	static std::span<const char * const> make_args(int argc, char * argv[]);

	static std::vector<const char *> make_args(
		std::span<const char *> baseArgs, int argc, char * argv[]);
};
