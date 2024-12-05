#pragma once

#include "arc/extra/guard.hpp"
#include "arc/fwd.hpp"
#include "arc/util/std.hpp"

struct arc::detail::globals
{
public:
	globals() = default;

	~globals();

	void add(arc::detail::handle && global);

private:
	arc::extra::shared_guard<std::stack<arc::detail::handle>> store;
};