#pragma once

#include "arc/fwd.hpp"
#include "arc/util/std.hpp"

#include <vector>

struct arc::options
{
	size_t workerThreadCount = 0;
	std::thread::id mainThreadId;
	std::vector<const char *> args;

	static options two_threads()
	{
		return from_args(
			std::array{ "--withMainThread", "true", "--workerThreadCount", "1" }, 0, nullptr);
	}

	static options from_args(std::span<const char * const> baseArgs, int argc, char * argv[]);
};
